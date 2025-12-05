# Singleton Pattern (单例模式)

## ASCII Diagram

```
+---------------------------+
|        Singleton          |
+---------------------------+
| - _instance: Singleton    |  (class variable, single instance)
| - _initialized: bool      |
+---------------------------+
| - __new__()               |  (controls instance creation)
| - __init__()              |
| + get_instance()          |  (alternative access method)
| + operation()             |
+---------------------------+

Instance Access Flow:
                                    
    Client 1 ----+                  +---------------------------+
                 |                  |        Singleton          |
    Client 2 ----+----> access ---->|  _instance ──────────────────> Single Object
                 |                  |                           |     in Memory
    Client 3 ----+                  +---------------------------+

All clients share the SAME instance
```

**中文说明：**
- **Singleton（单例类）**：确保只有一个实例，并提供全局访问点
- **_instance**：类变量，存储唯一实例
- **__new__ / get_instance**：控制实例创建，确保只创建一次
- **关键点**：所有客户端访问的是同一个对象实例

---

## 核心思想

确保一个类只有**一个实例**，并提供一个**全局访问点**来获取该实例。单例模式限制了类的实例化，确保在整个应用程序中共享同一个对象。

---

## 应用场景

1. **共享资源**：需要控制对共享资源的访问（数据库连接、文件系统）
2. **全局状态**：需要在整个应用中保持一致的状态
3. **控制实例数量**：某些类只需要一个实例（日志器、配置管理器）
4. **实际应用**：
   - 数据库连接池
   - 日志管理器
   - 配置管理器
   - 线程池
   - 缓存管理器
   - 应用程序设置

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 唯一实例 | 确保只有一个实例，节省内存 |
| 全局访问 | 提供全局访问点，方便使用 |
| 延迟初始化 | 可以在首次使用时才创建实例 |
| 控制资源 | 对共享资源的访问进行集中控制 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 违反单一职责 | 类同时管理自身实例和业务逻辑 |
| 难以测试 | 全局状态导致测试困难 |
| 隐藏依赖 | 组件间的依赖关系不明显 |
| 多线程问题 | 需要特别处理并发创建 |

---

## Python 代码示例

### 应用前：多个实例导致状态不一致

```python
# 问题：每次创建新实例，状态不共享

class DatabaseConnection:
    """数据库连接类 - 没有单例控制"""
    
    def __init__(self, host, port, database):
        self.host = host
        self.port = port
        self.database = database
        self.connected = False
        self._connection_count = 0
        print(f"Creating new connection to {host}:{port}/{database}")
    
    def connect(self):
        self._connection_count += 1
        self.connected = True
        print(f"Connected! (connection #{self._connection_count})")
    
    def query(self, sql):
        if not self.connected:
            raise RuntimeError("Not connected!")
        return f"Executing: {sql}"


class Logger:
    """日志类 - 没有单例控制"""
    
    def __init__(self, filename):
        self.filename = filename
        self.logs = []
        print(f"Creating new logger for {filename}")
    
    def log(self, message):
        self.logs.append(message)
        print(f"[{self.filename}] {message}")
    
    def get_logs(self):
        return self.logs


# 问题演示
print("=== Database Connection Problem ===")
db1 = DatabaseConnection("localhost", 5432, "mydb")
db1.connect()

db2 = DatabaseConnection("localhost", 5432, "mydb")  # 创建了新实例！
# db2 没有连接状态

print(f"\ndb1 connected: {db1.connected}")
print(f"db2 connected: {db2.connected}")  # False! 状态不共享
print(f"db1 is db2: {db1 is db2}")  # False，不是同一个对象

print("\n=== Logger Problem ===")
logger1 = Logger("app.log")
logger1.log("Application started")
logger1.log("User logged in")

logger2 = Logger("app.log")  # 创建了新实例！
logger2.log("Processing request")

print(f"\nlogger1 logs: {logger1.get_logs()}")
print(f"logger2 logs: {logger2.get_logs()}")  # 日志不共享！
```

### 应用后：使用单例模式

```python
import threading
from typing import Optional


# ========== 方式 1: 使用 __new__ 方法 ==========
class SingletonNew:
    """使用 __new__ 实现单例"""
    
    _instance: Optional['SingletonNew'] = None
    _lock = threading.Lock()
    
    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            with cls._lock:  # 线程安全
                if cls._instance is None:  # 双重检查
                    cls._instance = super().__new__(cls)
        return cls._instance
    
    def __init__(self, value=None):
        # 注意：__init__ 每次获取实例都会调用
        # 需要额外控制初始化逻辑
        if not hasattr(self, '_initialized'):
            self.value = value
            self._initialized = True


# ========== 方式 2: 使用装饰器 ==========
def singleton(cls):
    """单例装饰器"""
    instances = {}
    lock = threading.Lock()
    
    def get_instance(*args, **kwargs):
        if cls not in instances:
            with lock:
                if cls not in instances:
                    instances[cls] = cls(*args, **kwargs)
        return instances[cls]
    
    return get_instance


@singleton
class Logger:
    """日志单例"""
    
    def __init__(self, filename="app.log"):
        self.filename = filename
        self.logs = []
        print(f"Logger initialized with {filename}")
    
    def log(self, level: str, message: str):
        entry = f"[{level.upper()}] {message}"
        self.logs.append(entry)
        print(f"[{self.filename}] {entry}")
    
    def info(self, message: str):
        self.log("INFO", message)
    
    def error(self, message: str):
        self.log("ERROR", message)
    
    def get_logs(self):
        return self.logs.copy()


# ========== 方式 3: 使用元类 ==========
class SingletonMeta(type):
    """单例元类"""
    
    _instances = {}
    _lock = threading.Lock()
    
    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            with cls._lock:
                if cls not in cls._instances:
                    instance = super().__call__(*args, **kwargs)
                    cls._instances[cls] = instance
        return cls._instances[cls]


class DatabaseConnection(metaclass=SingletonMeta):
    """数据库连接单例"""
    
    def __init__(self, host="localhost", port=5432, database="mydb"):
        self.host = host
        self.port = port
        self.database = database
        self.connected = False
        self._query_count = 0
        print(f"Database connection initialized: {host}:{port}/{database}")
    
    def connect(self):
        if not self.connected:
            print(f"Connecting to {self.host}:{self.port}/{self.database}...")
            self.connected = True
            print("Connected successfully!")
    
    def disconnect(self):
        if self.connected:
            print("Disconnecting...")
            self.connected = False
    
    def query(self, sql: str) -> str:
        if not self.connected:
            raise RuntimeError("Not connected to database!")
        self._query_count += 1
        return f"Query #{self._query_count}: {sql}"
    
    def get_stats(self):
        return {
            "host": self.host,
            "connected": self.connected,
            "query_count": self._query_count
        }


# ========== 方式 4: 使用模块级变量（最 Pythonic）==========
# config_manager.py 模块内容:
"""
class _ConfigManager:
    def __init__(self):
        self.settings = {}
    
    def get(self, key, default=None):
        return self.settings.get(key, default)
    
    def set(self, key, value):
        self.settings[key] = value

# 模块级单例
config = _ConfigManager()
"""

# 使用: from config_manager import config


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 50)
    print("Testing Logger Singleton (Decorator)")
    print("=" * 50)
    
    # 获取 Logger 实例
    logger1 = Logger("app.log")
    logger1.info("Application started")
    logger1.info("User logged in")
    
    # 再次获取 - 返回同一个实例
    logger2 = Logger("different.log")  # 参数被忽略
    logger2.error("Something went wrong")
    
    print(f"\nlogger1 is logger2: {logger1 is logger2}")  # True
    print(f"All logs: {logger1.get_logs()}")  # 所有日志都在这里
    
    print("\n" + "=" * 50)
    print("Testing DatabaseConnection Singleton (Metaclass)")
    print("=" * 50)
    
    # 获取数据库连接
    db1 = DatabaseConnection("localhost", 5432, "mydb")
    db1.connect()
    print(db1.query("SELECT * FROM users"))
    print(db1.query("SELECT * FROM products"))
    
    # 再次获取 - 同一个实例
    db2 = DatabaseConnection("other-host", 3306, "otherdb")  # 参数被忽略
    print(f"\ndb1 is db2: {db1 is db2}")  # True
    print(f"db2 connected: {db2.connected}")  # True，共享状态
    print(db2.query("INSERT INTO orders ..."))
    
    print(f"\nDatabase stats: {db1.get_stats()}")
    
    print("\n" + "=" * 50)
    print("Testing Thread Safety")
    print("=" * 50)
    
    results = []
    
    def create_db():
        db = DatabaseConnection()
        results.append(id(db))
    
    threads = [threading.Thread(target=create_db) for _ in range(10)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    
    print(f"All instance IDs: {results}")
    print(f"All same instance: {len(set(results)) == 1}")  # True


# ========== 可测试的单例 ==========
class ConfigManager:
    """可测试的配置管理器"""
    
    _instance = None
    
    def __init__(self):
        self.settings = {}
    
    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance
    
    @classmethod
    def reset_instance(cls):
        """用于测试：重置单例"""
        cls._instance = None
    
    def get(self, key, default=None):
        return self.settings.get(key, default)
    
    def set(self, key, value):
        self.settings[key] = value
    
    def load_from_dict(self, config: dict):
        self.settings.update(config)


# 测试示例
def test_config_manager():
    # 测试前重置
    ConfigManager.reset_instance()
    
    config = ConfigManager.get_instance()
    config.set("debug", True)
    
    # 验证单例
    config2 = ConfigManager.get_instance()
    assert config2.get("debug") == True
    
    # 测试后清理
    ConfigManager.reset_instance()
    print("Test passed!")


test_config_manager()
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **实例数量** | 可能创建多个实例 | 确保只有一个实例 |
| **状态一致性** | 各实例状态独立，不同步 | 所有访问共享同一状态 |
| **资源使用** | 多次初始化，浪费资源 | 只初始化一次，节省资源 |
| **访问方式** | 需要传递对象引用 | 全局访问，任何地方都能获取 |
| **线程安全** | 需要额外处理 | 可以内置线程安全机制 |

---

## 各种实现方式对比

| 实现方式 | 优点 | 缺点 | 适用场景 |
|----------|------|------|----------|
| **__new__** | 标准、易理解 | __init__ 每次都调用 | 简单场景 |
| **装饰器** | 灵活、可复用 | 无法继承 | 多个单例类 |
| **元类** | 最标准、支持继承 | 复杂 | 需要继承的场景 |
| **模块变量** | 最 Pythonic、简单 | 无法延迟初始化 | 推荐方式 |

---

## 注意事项

```python
# 1. 单例与继承
class BaseSingleton(metaclass=SingletonMeta):
    pass

class ChildA(BaseSingleton):
    pass

class ChildB(BaseSingleton):
    pass

# 每个子类有自己的单例
a1 = ChildA()
a2 = ChildA()
b1 = ChildB()

print(a1 is a2)  # True
print(a1 is b1)  # False - 不同类的单例是独立的


# 2. 避免过度使用单例
# Bad: 所有东西都做成单例
# Good: 只在真正需要时使用

# 3. 依赖注入替代方案
class Service:
    def __init__(self, logger, db):
        self.logger = logger
        self.db = db

# 在应用启动时创建并注入依赖
# 而不是在 Service 内部获取单例
```

