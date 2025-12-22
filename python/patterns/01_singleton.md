# Singleton Pattern in Python

---

## 1. Pattern Name: Singleton

**Purpose / Problem Solved:**
Ensure a class has only one instance and provide a global point of access to it. Used for shared resources like configuration, logging, database connections, or thread pools.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
+------------------------------------------------------------------+
        |                    |                    |
        v                    v                    v
   get_instance()       get_instance()       get_instance()
        |                    |                    |
        +--------------------+--------------------+
                             |
                             v
              +------------------------------+
              |      SINGLETON INSTANCE      |
              |------------------------------|
              | - _instance: Singleton       |
              | - config: dict               |
              | - connection: Connection     |
              +------------------------------+
              |                              |
              | All calls return the SAME    |
              | instance (id matches)        |
              |                              |
              +------------------------------+

    First Call:                    Subsequent Calls:
    +------------------+           +------------------+
    | Create instance  |           | Return existing  |
    | Store in _instance|          | _instance        |
    +------------------+           +------------------+
```

**中文说明：**
单例模式确保一个类只有一个实例。无论客户端代码调用多少次get_instance()，返回的都是同一个对象（内存地址相同）。首次调用时创建实例并存储，后续调用直接返回已存储的实例。常用于配置管理、日志记录、数据库连接池等需要全局唯一的资源。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Singleton |
|----------------|---------------------------|
| **`__new__` method** | Controls instance creation before `__init__`, perfect for intercepting and returning existing instance |
| **Class variables** | Shared across all instances, ideal for storing the single instance reference |
| **Module-level variables** | Python modules are singletons by nature; importing returns the same module object |
| **Metaclasses** | Can control class instantiation behavior for all classes using the metaclass |
| **`__call__` in metaclass** | Intercepts class call (instantiation) to return cached instance |
| **Decorators** | Can wrap any class to add singleton behavior without modifying the class |
| **`functools.lru_cache`** | Built-in memoization can create function-based singletons |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Python `logging` module** | `logging.getLogger(name)` returns the same logger for the same name |
| **Django settings** | `django.conf.settings` is a lazy singleton for configuration |
| **SQLAlchemy Engine** | `create_engine()` typically returns a singleton per connection string |
| **Flask `current_app`** | Application context provides singleton-like access to the app |

```python
# Example from Python's logging module
import logging

logger1 = logging.getLogger("myapp")
logger2 = logging.getLogger("myapp")

assert logger1 is logger2  # Same instance!
```

---

## 5. Python Module Examples

### Method 1: Using `__new__` (Classic Approach)

```python
#!/usr/bin/env python3
"""
Singleton Pattern - Using __new__ method

This is the classic Python approach that overrides instance creation.
"""

class Singleton:
    """
    A classic Singleton implementation using __new__.
    
    The __new__ method is called before __init__ to create the instance.
    We override it to return the existing instance if one exists.
    """
    _instance = None
    
    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            # Create the instance only once
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self, value: str = "default"):
        # Prevent re-initialization
        if self._initialized:
            return
        self.value = value
        self._initialized = True
    
    def __repr__(self):
        return f"Singleton(value={self.value!r}, id={id(self)})"


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    # Create first instance
    s1 = Singleton("first")
    print(f"s1: {s1}")
    
    # Try to create second instance
    s2 = Singleton("second")  # "second" is ignored!
    print(f"s2: {s2}")
    
    # Verify they are the same instance
    print(f"\ns1 is s2: {s1 is s2}")
    print(f"id(s1) == id(s2): {id(s1) == id(s2)}")
    
    # Modify through one reference
    s1.value = "modified"
    print(f"\nAfter s1.value = 'modified':")
    print(f"s2.value: {s2.value}")  # Also modified!
```

**Output:**
```
s1: Singleton(value='first', id=140234567890)
s2: Singleton(value='first', id=140234567890)

s1 is s2: True
id(s1) == id(s2): True

After s1.value = 'modified':
s2.value: modified
```

---

### Method 2: Using Metaclass (Advanced)

```python
#!/usr/bin/env python3
"""
Singleton Pattern - Using Metaclass

Metaclasses control class creation. By overriding __call__,
we can intercept instantiation and return cached instances.
"""

class SingletonMeta(type):
    """
    A metaclass that creates Singleton classes.
    
    The __call__ method is invoked when the class is "called" (instantiated).
    We cache instances in a dictionary keyed by class.
    """
    _instances = {}
    
    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            # Create and cache the instance
            instance = super().__call__(*args, **kwargs)
            cls._instances[cls] = instance
        return cls._instances[cls]


class Database(metaclass=SingletonMeta):
    """Database connection singleton."""
    
    def __init__(self, connection_string: str = "localhost:5432"):
        self.connection_string = connection_string
        self.connected = False
    
    def connect(self):
        if not self.connected:
            print(f"Connecting to {self.connection_string}...")
            self.connected = True
        else:
            print("Already connected!")
    
    def query(self, sql: str):
        if self.connected:
            print(f"Executing: {sql}")
        else:
            raise RuntimeError("Not connected!")


class Logger(metaclass=SingletonMeta):
    """Logger singleton - different class, different instance."""
    
    def __init__(self, name: str = "app"):
        self.name = name
        self.messages = []
    
    def log(self, message: str):
        self.messages.append(message)
        print(f"[{self.name}] {message}")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    # Database singleton
    db1 = Database("postgres://prod:5432")
    db2 = Database("postgres://dev:5432")  # Ignored!
    
    print(f"db1 is db2: {db1 is db2}")
    print(f"Connection: {db1.connection_string}")
    
    db1.connect()
    db2.connect()  # Already connected via db1
    
    print()
    
    # Logger singleton (separate from Database)
    log1 = Logger("myapp")
    log2 = Logger("other")  # Ignored!
    
    print(f"log1 is log2: {log1 is log2}")
    log1.log("Hello")
    log2.log("World")
```

---

### Method 3: Using Decorator (Reusable)

```python
#!/usr/bin/env python3
"""
Singleton Pattern - Using Decorator

A decorator can wrap any class to add singleton behavior
without modifying the class itself.
"""

from functools import wraps


def singleton(cls):
    """
    Decorator that converts a class into a Singleton.
    
    Uses a closure to store the single instance.
    """
    instances = {}
    
    @wraps(cls)
    def get_instance(*args, **kwargs):
        if cls not in instances:
            instances[cls] = cls(*args, **kwargs)
        return instances[cls]
    
    return get_instance


@singleton
class Configuration:
    """Application configuration singleton."""
    
    def __init__(self):
        self.settings = {}
        self._load_defaults()
    
    def _load_defaults(self):
        self.settings = {
            "debug": False,
            "database_url": "sqlite:///app.db",
            "secret_key": "change-me",
        }
    
    def get(self, key: str, default=None):
        return self.settings.get(key, default)
    
    def set(self, key: str, value):
        self.settings[key] = value


@singleton
class ThreadPool:
    """Thread pool singleton."""
    
    def __init__(self, max_workers: int = 4):
        self.max_workers = max_workers
        self.tasks = []
        print(f"ThreadPool created with {max_workers} workers")
    
    def submit(self, task):
        self.tasks.append(task)
        print(f"Task submitted. Queue size: {len(self.tasks)}")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    # Configuration singleton
    config1 = Configuration()
    config2 = Configuration()
    
    print(f"config1 is config2: {config1 is config2}")
    
    config1.set("debug", True)
    print(f"config2.get('debug'): {config2.get('debug')}")  # True!
    
    print()
    
    # ThreadPool singleton
    pool1 = ThreadPool(8)  # Creates with 8 workers
    pool2 = ThreadPool(16)  # Ignored! Returns existing
    
    print(f"pool1 is pool2: {pool1 is pool2}")
    print(f"pool2.max_workers: {pool2.max_workers}")  # Still 8
    
    pool1.submit("task1")
    pool2.submit("task2")  # Same pool!
```

---

### Method 4: Module-Level Singleton (Pythonic)

```python
#!/usr/bin/env python3
"""
Singleton Pattern - Module-Level (Most Pythonic)

In Python, modules are singletons by nature.
Creating an instance at module level is the simplest approach.

This file can be imported, and `app_config` will be the same object.
"""

class _AppConfig:
    """
    Private class - users should use the module-level instance.
    
    The underscore prefix signals this is internal.
    """
    
    def __init__(self):
        self._settings = {}
        self._loaded = False
    
    def load(self, filepath: str = "config.json"):
        if self._loaded:
            print("Configuration already loaded")
            return
        
        # Simulate loading from file
        self._settings = {
            "app_name": "MyApp",
            "version": "1.0.0",
            "features": ["auth", "api", "admin"],
        }
        self._loaded = True
        print(f"Loaded configuration from {filepath}")
    
    def __getitem__(self, key: str):
        return self._settings[key]
    
    def __setitem__(self, key: str, value):
        self._settings[key] = value
    
    def __repr__(self):
        return f"AppConfig({self._settings})"


# THE SINGLETON INSTANCE
# This is created once when the module is first imported
app_config = _AppConfig()


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    # In practice, you would do:
    # from mymodule import app_config
    
    # First load
    app_config.load()
    
    # Second load - no-op
    app_config.load()
    
    print(f"\napp_config['app_name']: {app_config['app_name']}")
    print(f"app_config['features']: {app_config['features']}")
    
    # Modify
    app_config["debug"] = True
    print(f"\nAfter modification: {app_config}")
```

---

### Method 5: Thread-Safe Singleton

```python
#!/usr/bin/env python3
"""
Singleton Pattern - Thread-Safe Version

Uses threading.Lock to prevent race conditions when
multiple threads try to create the instance simultaneously.
"""

import threading
from typing import Optional


class ThreadSafeSingleton:
    """
    Thread-safe Singleton using double-checked locking.
    
    The lock ensures only one thread can create the instance,
    while subsequent access doesn't require acquiring the lock.
    """
    _instance: Optional["ThreadSafeSingleton"] = None
    _lock: threading.Lock = threading.Lock()
    
    def __new__(cls, *args, **kwargs):
        # First check (without lock) - fast path
        if cls._instance is None:
            # Acquire lock for thread safety
            with cls._lock:
                # Second check (with lock) - prevent race condition
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._initialized = False
        return cls._instance
    
    def __init__(self, name: str = "default"):
        with self._lock:
            if self._initialized:
                return
            self.name = name
            self.data = {}
            self._initialized = True
    
    def set_data(self, key: str, value):
        with self._lock:
            self.data[key] = value
    
    def get_data(self, key: str):
        with self._lock:
            return self.data.get(key)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    import concurrent.futures
    
    results = []
    
    def create_singleton(thread_id: int):
        """Function run by each thread."""
        instance = ThreadSafeSingleton(f"thread-{thread_id}")
        instance.set_data(f"key-{thread_id}", thread_id)
        return id(instance)
    
    # Create singleton from multiple threads simultaneously
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(create_singleton, i) for i in range(10)]
        results = [f.result() for f in futures]
    
    # All threads got the same instance
    print(f"All instance IDs equal: {len(set(results)) == 1}")
    print(f"Unique IDs: {set(results)}")
    
    # Check the singleton
    singleton = ThreadSafeSingleton()
    print(f"\nSingleton name: {singleton.name}")
    print(f"Singleton data: {singleton.data}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- Need exactly one instance (database connection pool, configuration)
- Need a global access point (logging)
- Instance creation is expensive and reuse is desired

### Avoid When:
- Testing becomes difficult (consider dependency injection instead)
- Different parts of the app need different configurations
- It introduces hidden global state that's hard to track

### Python Alternative:
In Python, prefer **module-level instances** or **dependency injection** over classic Singleton classes. They're simpler and more Pythonic.

```python
# Instead of Singleton class
# myconfig.py
config = load_config()  # Module-level instance

# usage.py
from myconfig import config  # Always same object
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Factory Method** | Often used to create the singleton instance |
| **Facade** | Singletons often serve as facades to subsystems |
| **Flyweight** | Manages shared instances, but multiple instances allowed |
| **Monostate/Borg** | Alternative that shares state instead of identity |

