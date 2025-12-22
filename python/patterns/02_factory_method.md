# Factory Method Pattern in Python

---

## 1. Pattern Name: Factory Method

**Purpose / Problem Solved:**
Define an interface for creating objects, but let subclasses decide which class to instantiate. Factory Method lets a class defer instantiation to subclasses. Decouples object creation from usage.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
+------------------------------------------------------------------+
                              |
                              | Uses factory method
                              v
+------------------------------------------------------------------+
|                        CREATOR (Abstract)                         |
|------------------------------------------------------------------|
| + factory_method() -> Product    # Abstract: subclasses override |
| + some_operation()               # Uses factory_method internally |
|------------------------------------------------------------------|
|   def some_operation(self):                                       |
|       product = self.factory_method()  # <-- Deferred creation   |
|       return product.do_work()                                    |
+------------------------------------------------------------------+
              ^                                    ^
              |                                    |
   +----------+----------+            +------------+-----------+
   |                     |            |                        |
+------------------+ +------------------+                       |
| ConcreteCreatorA | | ConcreteCreatorB |                       |
|------------------| |------------------|                       |
| factory_method() | | factory_method() |                       |
|   return         | |   return         |                       |
|   ProductA()     | |   ProductB()     |                       |
+------------------+ +------------------+                       |
         |                    |                                 |
         v                    v                                 |
+------------------+ +------------------+                       |
|    ProductA      | |    ProductB      | <-- Implement Product |
|------------------| |------------------|                       |
| + do_work()      | | + do_work()      |                       |
+------------------+ +------------------+                       |
                                                                |
+------------------------------------------------------------------+
|                       PRODUCT (Interface)                         |
|------------------------------------------------------------------|
| + do_work() -> Result                                            |
+------------------------------------------------------------------+
```

**中文说明：**
工厂方法模式将对象创建延迟到子类。Creator定义抽象的factory_method()，具体子类实现该方法返回不同的Product。客户端代码只依赖抽象的Creator和Product接口，不知道具体类型。这实现了"开闭原则"：添加新产品只需添加新的Creator子类，无需修改现有代码。Python中常用函数作为工厂，比类继承更简洁。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Factory Method |
|----------------|--------------------------------|
| **First-class functions** | Functions can be passed as factory callables |
| **`abc.ABC` and `@abstractmethod`** | Define abstract factory interfaces |
| **Duck typing** | Products don't need common base class if they share interface |
| **Class as first-class object** | Can pass class itself and call it to instantiate |
| **`__init_subclass__`** | Auto-register subclasses as they're defined |
| **`dict` as registry** | Map names/types to factory functions |
| **`*args, **kwargs`** | Forward arguments to the actual constructor |
| **Type hints** | `Callable[..., Product]` for factory type |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **SQLAlchemy** | `create_engine(url)` returns different dialect engines based on URL |
| **Django ORM** | `Model.objects.create()` factory method for model instances |
| **Requests** | `requests.request(method, url)` creates different request types |
| **Pathlib** | `Path()` returns `PosixPath` or `WindowsPath` based on OS |
| **json module** | `json.loads()` creates Python objects from JSON string |

```python
# Example from pathlib - Platform-aware factory
from pathlib import Path

# On Linux: returns PosixPath
# On Windows: returns WindowsPath
p = Path("/home/user")
print(type(p))  # <class 'pathlib.PosixPath'> on Linux
```

---

## 5. Python Module Examples

### Method 1: Classic Factory Method with ABC

```python
#!/usr/bin/env python3
"""
Factory Method Pattern - Classic Implementation with ABC

Uses abstract base classes to define the factory interface.
Subclasses override the factory method to create specific products.
"""

from abc import ABC, abstractmethod


# ============== PRODUCT HIERARCHY ==============

class Transport(ABC):
    """Abstract Product interface."""
    
    @abstractmethod
    def deliver(self, cargo: str) -> str:
        """Deliver the cargo."""
        pass


class Truck(Transport):
    """Concrete Product: Ground transport."""
    
    def deliver(self, cargo: str) -> str:
        return f"🚚 Delivering '{cargo}' by road in a truck"


class Ship(Transport):
    """Concrete Product: Sea transport."""
    
    def deliver(self, cargo: str) -> str:
        return f"🚢 Delivering '{cargo}' by sea in a ship"


class Airplane(Transport):
    """Concrete Product: Air transport."""
    
    def deliver(self, cargo: str) -> str:
        return f"✈️ Delivering '{cargo}' by air in an airplane"


# ============== CREATOR HIERARCHY ==============

class Logistics(ABC):
    """
    Abstract Creator with factory method.
    
    The factory method is abstract - subclasses must implement it.
    The plan_delivery method uses the factory method.
    """
    
    @abstractmethod
    def create_transport(self) -> Transport:
        """Factory method - subclasses decide which Transport to create."""
        pass
    
    def plan_delivery(self, cargo: str) -> str:
        """
        Business logic that uses the factory method.
        
        This method doesn't know the concrete Transport type.
        It works with any Transport returned by the factory.
        """
        # Call factory method to get the transport
        transport = self.create_transport()
        
        # Use the transport
        result = transport.deliver(cargo)
        return f"Logistics planned: {result}"


class RoadLogistics(Logistics):
    """Concrete Creator for road transport."""
    
    def create_transport(self) -> Transport:
        return Truck()


class SeaLogistics(Logistics):
    """Concrete Creator for sea transport."""
    
    def create_transport(self) -> Transport:
        return Ship()


class AirLogistics(Logistics):
    """Concrete Creator for air transport."""
    
    def create_transport(self) -> Transport:
        return Airplane()


# ============== CLIENT CODE ==============

def process_delivery(logistics: Logistics, cargo: str):
    """
    Client function that works with any Logistics type.
    
    It doesn't know or care which specific Transport is used.
    """
    print(logistics.plan_delivery(cargo))


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=== Factory Method Pattern Demo ===\n")
    
    # Client code works with abstract Logistics
    cargos = ["Electronics", "Furniture", "Medicine"]
    logistics_types = [RoadLogistics(), SeaLogistics(), AirLogistics()]
    
    for logistics, cargo in zip(logistics_types, cargos):
        process_delivery(logistics, cargo)
    
    print("\n=== Adding New Transport Type ===")
    
    # To add drone delivery, just create new classes:
    class Drone(Transport):
        def deliver(self, cargo: str) -> str:
            return f"🚁 Delivering '{cargo}' by drone"
    
    class DroneLogistics(Logistics):
        def create_transport(self) -> Transport:
            return Drone()
    
    # Works without changing existing code!
    process_delivery(DroneLogistics(), "Small package")
```

**Output:**
```
=== Factory Method Pattern Demo ===

Logistics planned: 🚚 Delivering 'Electronics' by road in a truck
Logistics planned: 🚢 Delivering 'Furniture' by sea in a ship
Logistics planned: ✈️ Delivering 'Medicine' by air in an airplane

=== Adding New Transport Type ===
Logistics planned: 🚁 Delivering 'Small package' by drone
```

---

### Method 2: Simple Factory (Pythonic with Functions)

```python
#!/usr/bin/env python3
"""
Simple Factory - Pythonic Approach with Functions

In Python, factory methods are often just functions.
Uses dict-based dispatch for clean, extensible design.
"""

from dataclasses import dataclass
from typing import Callable, Protocol


# ============== PRODUCT PROTOCOL ==============

class Serializer(Protocol):
    """Protocol defining what a serializer must do."""
    
    def serialize(self, data: dict) -> str:
        """Serialize data to string format."""
        ...


# ============== CONCRETE PRODUCTS ==============

@dataclass
class JsonSerializer:
    """JSON serializer."""
    indent: int = 2
    
    def serialize(self, data: dict) -> str:
        import json
        return json.dumps(data, indent=self.indent)


@dataclass
class XmlSerializer:
    """XML serializer."""
    root_tag: str = "root"
    
    def serialize(self, data: dict) -> str:
        lines = [f"<{self.root_tag}>"]
        for key, value in data.items():
            lines.append(f"  <{key}>{value}</{key}>")
        lines.append(f"</{self.root_tag}>")
        return "\n".join(lines)


@dataclass
class YamlSerializer:
    """YAML serializer."""
    
    def serialize(self, data: dict) -> str:
        lines = []
        for key, value in data.items():
            if isinstance(value, list):
                lines.append(f"{key}:")
                for item in value:
                    lines.append(f"  - {item}")
            else:
                lines.append(f"{key}: {value}")
        return "\n".join(lines)


# ============== FACTORY ==============

# Registry mapping format names to factory functions
_serializer_registry: dict[str, Callable[..., Serializer]] = {
    "json": JsonSerializer,
    "xml": XmlSerializer,
    "yaml": YamlSerializer,
}


def register_serializer(name: str, factory: Callable[..., Serializer]):
    """Register a new serializer factory."""
    _serializer_registry[name] = factory


def create_serializer(format_type: str, **kwargs) -> Serializer:
    """
    Factory function that creates serializers.
    
    Args:
        format_type: One of 'json', 'xml', 'yaml', or custom registered types
        **kwargs: Additional arguments passed to the serializer constructor
    
    Returns:
        A serializer instance
    
    Raises:
        ValueError: If format_type is not registered
    """
    if format_type not in _serializer_registry:
        available = ", ".join(_serializer_registry.keys())
        raise ValueError(f"Unknown format '{format_type}'. Available: {available}")
    
    factory = _serializer_registry[format_type]
    return factory(**kwargs)


def get_available_formats() -> list[str]:
    """Return list of available serializer formats."""
    return list(_serializer_registry.keys())


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=== Simple Factory Pattern Demo ===\n")
    
    data = {
        "name": "Alice",
        "age": 30,
        "city": "New York"
    }
    
    # Create different serializers using the factory
    for format_type in get_available_formats():
        print(f"--- {format_type.upper()} ---")
        serializer = create_serializer(format_type)
        print(serializer.serialize(data))
        print()
    
    # Create with custom options
    print("--- JSON with indent=4 ---")
    json_serializer = create_serializer("json", indent=4)
    print(json_serializer.serialize(data))
    print()
    
    # Extend with new serializer (Open/Closed Principle)
    print("=== Registering Custom Serializer ===\n")
    
    @dataclass
    class CsvSerializer:
        delimiter: str = ","
        
        def serialize(self, data: dict) -> str:
            keys = self.delimiter.join(data.keys())
            values = self.delimiter.join(str(v) for v in data.values())
            return f"{keys}\n{values}"
    
    # Register new serializer
    register_serializer("csv", CsvSerializer)
    
    print("--- CSV ---")
    csv_serializer = create_serializer("csv")
    print(csv_serializer.serialize(data))
```

---

### Method 3: Factory with Auto-Registration

```python
#!/usr/bin/env python3
"""
Factory Method with Auto-Registration

Uses __init_subclass__ for automatic registration of new types.
No manual registration needed - just define the subclass!
"""

from abc import ABC, abstractmethod
from typing import ClassVar


class Plugin(ABC):
    """
    Abstract base class with auto-registration.
    
    Subclasses are automatically registered when defined.
    """
    
    # Class-level registry
    _registry: ClassVar[dict[str, type["Plugin"]]] = {}
    
    # Each subclass must define its name
    name: ClassVar[str]
    
    def __init_subclass__(cls, **kwargs):
        """Called when a subclass is defined."""
        super().__init_subclass__(**kwargs)
        
        # Register the subclass if it has a name
        if hasattr(cls, "name") and cls.name:
            Plugin._registry[cls.name] = cls
            print(f"Registered plugin: {cls.name}")
    
    @classmethod
    def create(cls, name: str, *args, **kwargs) -> "Plugin":
        """Factory method to create plugins by name."""
        if name not in cls._registry:
            available = ", ".join(cls._registry.keys())
            raise ValueError(f"Unknown plugin '{name}'. Available: {available}")
        
        plugin_class = cls._registry[name]
        return plugin_class(*args, **kwargs)
    
    @classmethod
    def list_plugins(cls) -> list[str]:
        """List all registered plugins."""
        return list(cls._registry.keys())
    
    @abstractmethod
    def execute(self, data: str) -> str:
        """Execute the plugin's main functionality."""
        pass


# ============== CONCRETE PLUGINS ==============

class UppercasePlugin(Plugin):
    """Plugin that converts text to uppercase."""
    name = "uppercase"
    
    def execute(self, data: str) -> str:
        return data.upper()


class ReversePlugin(Plugin):
    """Plugin that reverses text."""
    name = "reverse"
    
    def execute(self, data: str) -> str:
        return data[::-1]


class Base64Plugin(Plugin):
    """Plugin that encodes text as base64."""
    name = "base64"
    
    def execute(self, data: str) -> str:
        import base64
        return base64.b64encode(data.encode()).decode()


class HashPlugin(Plugin):
    """Plugin that hashes text."""
    name = "hash"
    
    def __init__(self, algorithm: str = "sha256"):
        self.algorithm = algorithm
    
    def execute(self, data: str) -> str:
        import hashlib
        h = hashlib.new(self.algorithm)
        h.update(data.encode())
        return h.hexdigest()


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("\n=== Plugin Factory Demo ===\n")
    
    # List available plugins
    print(f"Available plugins: {Plugin.list_plugins()}")
    print()
    
    text = "Hello, World!"
    
    # Use factory to create and execute plugins
    for plugin_name in Plugin.list_plugins():
        plugin = Plugin.create(plugin_name)
        result = plugin.execute(text)
        print(f"{plugin_name}: {result}")
    
    print()
    
    # Create with arguments
    md5_plugin = Plugin.create("hash", algorithm="md5")
    print(f"MD5 hash: {md5_plugin.execute(text)}")
    
    print()
    
    # Add new plugin at runtime
    class CompressPlugin(Plugin):
        name = "compress"
        
        def execute(self, data: str) -> str:
            import zlib
            compressed = zlib.compress(data.encode())
            return f"Compressed {len(data)} bytes to {len(compressed)} bytes"
    
    print()
    print(f"Updated plugins: {Plugin.list_plugins()}")
    print(Plugin.create("compress").execute(text))
```

---

### Method 4: Parameterized Factory

```python
#!/usr/bin/env python3
"""
Parameterized Factory Pattern

Factory that creates objects based on configuration parameters.
Common in frameworks for creating configured instances.
"""

from dataclasses import dataclass, field
from typing import Any
from enum import Enum, auto


class DatabaseType(Enum):
    """Supported database types."""
    SQLITE = auto()
    POSTGRES = auto()
    MYSQL = auto()
    MONGODB = auto()


@dataclass
class DatabaseConfig:
    """Configuration for database connection."""
    db_type: DatabaseType
    host: str = "localhost"
    port: int = 0  # 0 means use default
    database: str = "app"
    username: str = ""
    password: str = ""
    options: dict[str, Any] = field(default_factory=dict)
    
    def __post_init__(self):
        # Set default ports
        if self.port == 0:
            defaults = {
                DatabaseType.SQLITE: 0,
                DatabaseType.POSTGRES: 5432,
                DatabaseType.MYSQL: 3306,
                DatabaseType.MONGODB: 27017,
            }
            self.port = defaults.get(self.db_type, 0)


class DatabaseConnection:
    """Base database connection class."""
    
    def __init__(self, config: DatabaseConfig):
        self.config = config
    
    def connect(self) -> str:
        raise NotImplementedError
    
    def connection_string(self) -> str:
        raise NotImplementedError


class SQLiteConnection(DatabaseConnection):
    """SQLite database connection."""
    
    def connect(self) -> str:
        return f"Connected to SQLite: {self.config.database}"
    
    def connection_string(self) -> str:
        return f"sqlite:///{self.config.database}.db"


class PostgresConnection(DatabaseConnection):
    """PostgreSQL database connection."""
    
    def connect(self) -> str:
        return f"Connected to PostgreSQL: {self.config.host}:{self.config.port}"
    
    def connection_string(self) -> str:
        auth = ""
        if self.config.username:
            auth = f"{self.config.username}:{self.config.password}@"
        return f"postgresql://{auth}{self.config.host}:{self.config.port}/{self.config.database}"


class MySQLConnection(DatabaseConnection):
    """MySQL database connection."""
    
    def connect(self) -> str:
        return f"Connected to MySQL: {self.config.host}:{self.config.port}"
    
    def connection_string(self) -> str:
        auth = ""
        if self.config.username:
            auth = f"{self.config.username}:{self.config.password}@"
        return f"mysql://{auth}{self.config.host}:{self.config.port}/{self.config.database}"


class MongoDBConnection(DatabaseConnection):
    """MongoDB database connection."""
    
    def connect(self) -> str:
        return f"Connected to MongoDB: {self.config.host}:{self.config.port}"
    
    def connection_string(self) -> str:
        auth = ""
        if self.config.username:
            auth = f"{self.config.username}:{self.config.password}@"
        return f"mongodb://{auth}{self.config.host}:{self.config.port}/{self.config.database}"


class DatabaseFactory:
    """
    Parameterized factory for creating database connections.
    
    Creates the appropriate connection type based on configuration.
    """
    
    _connection_classes = {
        DatabaseType.SQLITE: SQLiteConnection,
        DatabaseType.POSTGRES: PostgresConnection,
        DatabaseType.MYSQL: MySQLConnection,
        DatabaseType.MONGODB: MongoDBConnection,
    }
    
    @classmethod
    def create(cls, config: DatabaseConfig) -> DatabaseConnection:
        """Create a database connection from configuration."""
        connection_class = cls._connection_classes.get(config.db_type)
        
        if connection_class is None:
            raise ValueError(f"Unsupported database type: {config.db_type}")
        
        return connection_class(config)
    
    @classmethod
    def create_from_url(cls, url: str) -> DatabaseConnection:
        """Create a database connection from a URL string."""
        # Parse URL to determine type and config
        if url.startswith("sqlite"):
            config = DatabaseConfig(DatabaseType.SQLITE, database=url.split("///")[1].replace(".db", ""))
        elif url.startswith("postgresql"):
            config = DatabaseConfig(DatabaseType.POSTGRES)
        elif url.startswith("mysql"):
            config = DatabaseConfig(DatabaseType.MYSQL)
        elif url.startswith("mongodb"):
            config = DatabaseConfig(DatabaseType.MONGODB)
        else:
            raise ValueError(f"Cannot parse database URL: {url}")
        
        return cls.create(config)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=== Parameterized Factory Demo ===\n")
    
    # Create different database connections from config
    configs = [
        DatabaseConfig(DatabaseType.SQLITE, database="myapp"),
        DatabaseConfig(DatabaseType.POSTGRES, host="db.example.com", database="production", 
                      username="admin", password="secret"),
        DatabaseConfig(DatabaseType.MYSQL, database="wordpress"),
        DatabaseConfig(DatabaseType.MONGODB, database="analytics"),
    ]
    
    for config in configs:
        connection = DatabaseFactory.create(config)
        print(f"Type: {config.db_type.name}")
        print(f"  Connection string: {connection.connection_string()}")
        print(f"  {connection.connect()}")
        print()
    
    # Create from URL
    print("=== Create from URL ===\n")
    url = "sqlite:///test.db"
    conn = DatabaseFactory.create_from_url(url)
    print(f"From URL '{url}': {conn.connect()}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- You don't know exact types and dependencies beforehand
- You want to provide a library/framework that users can extend
- You need to decouple creation logic from usage logic
- You want to follow the Open/Closed Principle

### Avoid When:
- You always know the exact type to create (just use constructor)
- Adding new types requires changes to the factory (use registry instead)
- The pattern adds unnecessary complexity for simple cases

### Python Idiom:
In Python, prefer **simple factory functions** over class hierarchies:

```python
# Pythonic: Simple function factory
def create_connection(db_type: str) -> Connection:
    factories = {"mysql": MySQL, "postgres": Postgres}
    return factories[db_type]()

# vs Classical: Class hierarchy (often overkill in Python)
class ConnectionFactory(ABC):
    @abstractmethod
    def create(self) -> Connection: ...
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Abstract Factory** | Factory of factories - creates families of related objects |
| **Template Method** | Uses factory method in a template algorithm |
| **Prototype** | Alternative to factory - clone existing objects |
| **Builder** | More control over complex object construction |

