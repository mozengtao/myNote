# Design Patterns - Creational Patterns (创建型模式)

A comprehensive guide to creational design patterns with English explanations,
Chinese details, ASCII diagrams, and Python code examples.

---

## Table of Contents

1. [Singleton Pattern (单例模式)](#1-singleton-pattern-单例模式)
2. [Simple Factory Pattern (简单工厂模式)](#2-simple-factory-pattern-简单工厂模式)
3. [Factory Method Pattern (工厂方法模式)](#3-factory-method-pattern-工厂方法模式)
4. [Abstract Factory Pattern (抽象工厂模式)](#4-abstract-factory-pattern-抽象工厂模式)
5. [Builder Pattern (生成器模式)](#5-builder-pattern-生成器模式)
6. [Prototype Pattern (原型模式)](#6-prototype-pattern-原型模式)

---

## 1. Singleton Pattern (单例模式)

**Ensure a class has only one instance and provide a global point of access to it.**

### 中文详解

单例模式是一种创建型设计模式，它确保一个类只有一个实例，并提供一个全局访问点来获取该实例。

**适用场景：**
- 当类只能有一个实例，而且客户可以从一个众所周知的访问点访问它时
- 当这个唯一实例应该是通过子类化可扩展的，而且客户无需更改代码就能使用扩展后的实例时
- 例如：数据库连接池、日志记录器、配置管理器、线程池

**优点：**
- 保证一个类只有一个实例
- 获得了一个指向该实例的全局访问节点
- 仅在首次请求时初始化（延迟初始化）

**缺点：**
- 违反了单一职责原则（该模式同时解决了两个问题）
- 可能掩盖不良设计（组件之间相互了解过多）
- 多线程环境下需要特殊处理
- 单元测试困难

### Structure Diagram

```
+------------------------------------------+
|              Singleton                   |
+------------------------------------------+
| - _instance: Singleton = None            |
| - _lock: Lock                            |
+------------------------------------------+
| - __init__()                             |
| + get_instance(): Singleton              |
| + some_business_logic()                  |
+------------------------------------------+
          ^
          |
          | creates and returns
          |
+------------------------------------------+
|              Client                      |
+------------------------------------------+
| Singleton.get_instance()                 |
+------------------------------------------+
```

**图解说明：**
- Singleton 类包含一个私有静态变量 `_instance` 用于存储唯一实例
- `_lock` 用于线程安全的实例创建
- `get_instance()` 是获取单例的公共静态方法
- 客户端通过 `get_instance()` 获取实例，而不是直接 `new`

### Python Code Example

```python
"""
Singleton Pattern Implementation in Python
单例模式的 Python 实现
"""

from threading import Lock
from typing import Optional


class SingletonMeta(type):
    """
    Thread-safe Singleton metaclass implementation.
    线程安全的单例元类实现。
    """
    _instances: dict = {}
    _lock: Lock = Lock()

    def __call__(cls, *args, **kwargs):
        # Double-checked locking pattern
        # 双重检查锁定模式
        if cls not in cls._instances:
            with cls._lock:
                if cls not in cls._instances:
                    instance = super().__call__(*args, **kwargs)
                    cls._instances[cls] = instance
        return cls._instances[cls]


class DatabaseConnection(metaclass=SingletonMeta):
    """
    Example: Database connection as a singleton.
    示例：数据库连接作为单例。
    """
    def __init__(self, host: str = "localhost", port: int = 5432):
        self.host = host
        self.port = port
        self._connection = None
        print(f"DatabaseConnection initialized: {host}:{port}")

    def connect(self) -> str:
        if self._connection is None:
            self._connection = f"Connection to {self.host}:{self.port}"
        return self._connection

    def query(self, sql: str) -> str:
        return f"Executing: {sql} on {self._connection}"


# Alternative: Simple decorator-based singleton
# 另一种方式：基于装饰器的简单单例

def singleton(cls):
    """Decorator to make a class a singleton."""
    instances = {}
    lock = Lock()

    def get_instance(*args, **kwargs):
        if cls not in instances:
            with lock:
                if cls not in instances:
                    instances[cls] = cls(*args, **kwargs)
        return instances[cls]

    return get_instance


@singleton
class Logger:
    """Example: Logger as a singleton using decorator."""
    def __init__(self):
        self.logs = []
        print("Logger initialized")

    def log(self, message: str):
        self.logs.append(message)
        print(f"[LOG] {message}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Singleton Pattern Demo ===\n")

    # Test metaclass-based singleton
    print("1. Testing DatabaseConnection singleton:")
    db1 = DatabaseConnection("db.example.com", 3306)
    db2 = DatabaseConnection("other.host.com", 5432)  # Same instance returned

    print(f"   db1 is db2: {db1 is db2}")  # True
    print(f"   db1.host: {db1.host}")      # db.example.com (first init wins)
    print()

    # Test decorator-based singleton
    print("2. Testing Logger singleton:")
    logger1 = Logger()
    logger2 = Logger()

    print(f"   logger1 is logger2: {logger1 is logger2}")  # True
    logger1.log("First message")
    logger2.log("Second message")
    print(f"   Total logs: {len(logger1.logs)}")  # 2
```

---

## 2. Simple Factory Pattern (简单工厂模式)

**A factory class decides which product class to instantiate based on input parameters.**

### 中文详解

简单工厂模式（也称为静态工厂方法）是一种创建型设计模式，它提供一个创建对象的接口，但由工厂类决定实例化哪个类。简单工厂把实例化的操作单独放到一个类中，这个类就成为简单工厂类，让简单工厂类来决定应该用哪个具体子类来实例化。

**适用场景：**
- 当创建逻辑比较简单，不需要扩展时
- 客户端只需要知道传入工厂类的参数，而不关心如何创建对象
- 工厂类负责的对象比较少时

**优点：**
- 工厂类包含必要的逻辑判断，可以决定在什么时候创建哪一个产品类的实例
- 客户端可以免除直接创建产品对象的职责
- 实现了对象创建和使用的分离

**缺点：**
- 工厂类职责过重，一旦工厂类出问题，整个系统都会受到影响
- 增加新产品时需要修改工厂类的判断逻辑，违反开闭原则
- 不利于扩展复杂的产品结构

**注意：** 简单工厂模式不是 GoF 23 种设计模式之一，但它是工厂模式家族的基础。

### Structure Diagram

```
                    +-------------------+
                    |   SimpleFactory   |
                    +-------------------+
                    | + create_product()|----+
                    +-------------------+    |
                             |               |
                             | creates       |
                             v               |
                    +-------------------+    |
                    |     Product       |<---+
                    |    <<abstract>>   |
                    +-------------------+
                    | + operation()     |
                    +-------------------+
                             ^
                             |
          +------------------+------------------+
          |                  |                  |
+-----------------+  +-----------------+  +-----------------+
|   ProductA      |  |   ProductB      |  |   ProductC      |
+-----------------+  +-----------------+  +-----------------+
| + operation()   |  | + operation()   |  | + operation()   |
+-----------------+  +-----------------+  +-----------------+
```

**图解说明：**
- `SimpleFactory` 是工厂类，包含创建产品的静态方法
- `Product` 是抽象产品接口，定义产品的公共接口
- `ProductA`、`ProductB`、`ProductC` 是具体产品类
- 客户端通过工厂类创建产品，无需知道具体产品类

### Python Code Example

```python
"""
Simple Factory Pattern Implementation in Python
简单工厂模式的 Python 实现
"""

from abc import ABC, abstractmethod
from enum import Enum


class VehicleType(Enum):
    """Vehicle type enumeration."""
    CAR = "car"
    MOTORCYCLE = "motorcycle"
    TRUCK = "truck"


class Vehicle(ABC):
    """
    Abstract product class.
    抽象产品类：定义所有车辆的公共接口。
    """
    @abstractmethod
    def drive(self) -> str:
        """Drive the vehicle."""
        pass

    @abstractmethod
    def get_wheels(self) -> int:
        """Get number of wheels."""
        pass

    @abstractmethod
    def get_description(self) -> str:
        """Get vehicle description."""
        pass


class Car(Vehicle):
    """Concrete product: Car."""
    def __init__(self, brand: str = "Generic"):
        self.brand = brand

    def drive(self) -> str:
        return f"Driving {self.brand} car on the road"

    def get_wheels(self) -> int:
        return 4

    def get_description(self) -> str:
        return f"{self.brand} Car - 4 wheels, comfortable ride"


class Motorcycle(Vehicle):
    """Concrete product: Motorcycle."""
    def __init__(self, brand: str = "Generic"):
        self.brand = brand

    def drive(self) -> str:
        return f"Riding {self.brand} motorcycle fast!"

    def get_wheels(self) -> int:
        return 2

    def get_description(self) -> str:
        return f"{self.brand} Motorcycle - 2 wheels, exciting ride"


class Truck(Vehicle):
    """Concrete product: Truck."""
    def __init__(self, brand: str = "Generic", capacity_tons: int = 10):
        self.brand = brand
        self.capacity = capacity_tons

    def drive(self) -> str:
        return f"Driving {self.brand} truck carrying {self.capacity} tons"

    def get_wheels(self) -> int:
        return 6

    def get_description(self) -> str:
        return f"{self.brand} Truck - 6 wheels, {self.capacity}T capacity"


class VehicleFactory:
    """
    Simple Factory class.
    简单工厂类：根据参数创建不同类型的车辆。
    """
    @staticmethod
    def create_vehicle(vehicle_type: VehicleType, brand: str = "Generic", 
                       **kwargs) -> Vehicle:
        """
        Create a vehicle based on the type.
        根据类型创建车辆。
        """
        if vehicle_type == VehicleType.CAR:
            return Car(brand)
        elif vehicle_type == VehicleType.MOTORCYCLE:
            return Motorcycle(brand)
        elif vehicle_type == VehicleType.TRUCK:
            capacity = kwargs.get("capacity_tons", 10)
            return Truck(brand, capacity)
        else:
            raise ValueError(f"Unknown vehicle type: {vehicle_type}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Simple Factory Pattern Demo ===\n")

    # Create different vehicles using the factory
    vehicles = [
        VehicleFactory.create_vehicle(VehicleType.CAR, "Toyota"),
        VehicleFactory.create_vehicle(VehicleType.MOTORCYCLE, "Harley-Davidson"),
        VehicleFactory.create_vehicle(VehicleType.TRUCK, "Volvo", capacity_tons=20),
    ]

    for vehicle in vehicles:
        print(f"Description: {vehicle.get_description()}")
        print(f"Wheels: {vehicle.get_wheels()}")
        print(f"Action: {vehicle.drive()}")
        print()
```

---

## 3. Factory Method Pattern (工厂方法模式)

**Define an interface for creating an object, but let subclasses decide which class to instantiate.**

### 中文详解

工厂方法模式是一种创建型设计模式，它定义了一个创建对象的接口，但由子类决定要实例化的类是哪一个。工厂方法让类把实例化推迟到子类。

**与简单工厂的区别：**
- 简单工厂：一个工厂类，通过参数决定创建哪种产品
- 工厂方法：多个工厂类，每个工厂负责创建一种产品

**适用场景：**
- 当一个类不知道它所需要的对象的类时
- 当一个类希望由它的子类来指定它所创建的对象时
- 当类将创建对象的职责委托给多个帮助子类中的某一个时

**优点：**
- 符合开闭原则：添加新产品时，只需添加新的具体产品类和对应的具体工厂类
- 符合单一职责原则：每个具体工厂类只负责创建对应的产品
- 客户端只需要知道抽象工厂和抽象产品

**缺点：**
- 每增加一个产品，就需要增加一个具体产品类和一个具体工厂类，增加系统复杂度

### Structure Diagram

```
+---------------------+              +---------------------+
|      Creator        |              |      Product        |
|    <<abstract>>     |              |    <<abstract>>     |
+---------------------+              +---------------------+
| + factory_method()  |------------->| + operation()       |
| + some_operation()  |   creates    +---------------------+
+---------------------+                       ^
          ^                                   |
          |                    +--------------+--------------+
          |                    |                             |
+---------------------+  +---------------------+  +---------------------+
|  ConcreteCreatorA   |  |  ConcreteProductA   |  |  ConcreteProductB   |
+---------------------+  +---------------------+  +---------------------+
| + factory_method()  |  | + operation()       |  | + operation()       |
+---------------------+  +---------------------+  +---------------------+
          |                        ^
          |  creates               |
          +------------------------+

+---------------------+
|  ConcreteCreatorB   |
+---------------------+
| + factory_method()  |------ creates ------> ConcreteProductB
+---------------------+
```

**图解说明：**
- `Creator` 是抽象创建者，声明工厂方法 `factory_method()`
- `ConcreteCreatorA/B` 是具体创建者，实现工厂方法返回具体产品
- `Product` 是抽象产品接口
- `ConcreteProductA/B` 是具体产品类
- 每个具体创建者对应创建一种具体产品

### Python Code Example

```python
"""
Factory Method Pattern Implementation in Python
工厂方法模式的 Python 实现
"""

from abc import ABC, abstractmethod


# Abstract Product
class Document(ABC):
    """
    Abstract product interface.
    抽象产品接口：定义文档的公共操作。
    """
    @abstractmethod
    def create(self) -> str:
        pass

    @abstractmethod
    def save(self, filename: str) -> str:
        pass

    @abstractmethod
    def get_extension(self) -> str:
        pass


# Concrete Products
class PDFDocument(Document):
    """Concrete product: PDF Document."""
    def __init__(self):
        self.content = []

    def create(self) -> str:
        return "Created new PDF document with header and footer"

    def save(self, filename: str) -> str:
        return f"Saved PDF document as {filename}.{self.get_extension()}"

    def get_extension(self) -> str:
        return "pdf"


class WordDocument(Document):
    """Concrete product: Word Document."""
    def __init__(self):
        self.content = []

    def create(self) -> str:
        return "Created new Word document with default template"

    def save(self, filename: str) -> str:
        return f"Saved Word document as {filename}.{self.get_extension()}"

    def get_extension(self) -> str:
        return "docx"


class ExcelDocument(Document):
    """Concrete product: Excel Document."""
    def __init__(self):
        self.sheets = []

    def create(self) -> str:
        return "Created new Excel spreadsheet with Sheet1"

    def save(self, filename: str) -> str:
        return f"Saved Excel document as {filename}.{self.get_extension()}"

    def get_extension(self) -> str:
        return "xlsx"


# Abstract Creator
class Application(ABC):
    """
    Abstract creator class.
    抽象创建者类：定义工厂方法和使用产品的操作。
    """
    @abstractmethod
    def create_document(self) -> Document:
        """
        Factory method - subclasses must implement.
        工厂方法 - 子类必须实现。
        """
        pass

    def new_document(self) -> str:
        """
        Template method that uses the factory method.
        使用工厂方法的模板方法。
        """
        # Call factory method to get a product
        document = self.create_document()

        # Use the product
        result = []
        result.append(document.create())
        result.append(f"Document type: {document.get_extension().upper()}")
        return "\n".join(result)

    def save_document(self, filename: str) -> str:
        document = self.create_document()
        return document.save(filename)


# Concrete Creators
class PDFApplication(Application):
    """Concrete creator for PDF documents."""
    def create_document(self) -> Document:
        return PDFDocument()


class WordApplication(Application):
    """Concrete creator for Word documents."""
    def create_document(self) -> Document:
        return WordDocument()


class ExcelApplication(Application):
    """Concrete creator for Excel documents."""
    def create_document(self) -> Document:
        return ExcelDocument()


def client_code(app: Application, filename: str):
    """
    Client code works with any application through the abstract interface.
    客户端代码通过抽象接口与任何应用程序协作。
    """
    print(app.new_document())
    print(app.save_document(filename))
    print()


# Client code demonstration
if __name__ == "__main__":
    print("=== Factory Method Pattern Demo ===\n")

    print("1. Working with PDF Application:")
    client_code(PDFApplication(), "report")

    print("2. Working with Word Application:")
    client_code(WordApplication(), "letter")

    print("3. Working with Excel Application:")
    client_code(ExcelApplication(), "budget")
```

---

## 4. Abstract Factory Pattern (抽象工厂模式)

**Provide an interface for creating families of related or dependent objects without specifying their concrete classes.**

### 中文详解

抽象工厂模式是一种创建型设计模式，它提供一个接口，用于创建一系列相关或相互依赖的对象，而无需指定它们的具体类。

**与工厂方法的区别：**
- 工厂方法：一个工厂创建一种产品
- 抽象工厂：一个工厂创建一个产品族（多种相关产品）

**适用场景：**
- 当系统需要独立于它的产品的创建、组合和表示时
- 当系统需要由多个产品系列中的一个来配置时
- 当需要强调一系列相关产品对象的设计以便进行联合使用时
- 当需要提供一个产品类库，只想显示它们的接口而不是实现时
- 例如：跨平台 UI 工具包（Windows/Mac/Linux 风格的按钮、菜单、对话框）

**优点：**
- 确保同一产品族的对象一起使用
- 符合开闭原则：添加新的产品族容易
- 符合单一职责原则：产品创建代码集中

**缺点：**
- 难以支持新种类的产品（需要修改抽象工厂接口及所有子类）

### Structure Diagram

```
+---------------------------+         +---------------------------+
|     AbstractFactory       |         |      AbstractProductA     |
|       <<interface>>       |         |       <<interface>>       |
+---------------------------+         +---------------------------+
| + create_product_a()      |-------->| + operation_a()           |
| + create_product_b()      |         +---------------------------+
+---------------------------+                     ^
          ^                                       |
          |                        +--------------+--------------+
          |                        |                             |
+------------------+      +------------------+          +------------------+
| ConcreteFactory1 |      | ProductA1        |          | ProductA2        |
+------------------+      +------------------+          +------------------+
| +create_product_a|----->| + operation_a()  |          | + operation_a()  |
| +create_product_b|      +------------------+          +------------------+
+------------------+
          |
          |         +---------------------------+
          |         |      AbstractProductB     |
          |         |       <<interface>>       |
          +-------->+---------------------------+
                    | + operation_b()           |
                    +---------------------------+
                                ^
                                |
                 +--------------+--------------+
                 |                             |
        +------------------+          +------------------+
        | ProductB1        |          | ProductB2        |
        +------------------+          +------------------+
        | + operation_b()  |          | + operation_b()  |
        +------------------+          +------------------+
```

**图解说明：**
- `AbstractFactory` 声明创建各种产品的方法
- `ConcreteFactory1/2` 实现创建具体产品族的方法
- `AbstractProductA/B` 定义不同种类产品的接口
- `ProductA1/B1` 属于同一产品族（由 ConcreteFactory1 创建）
- `ProductA2/B2` 属于另一产品族（由 ConcreteFactory2 创建）

### Python Code Example

```python
"""
Abstract Factory Pattern Implementation in Python
抽象工厂模式的 Python 实现

Example: Cross-platform UI components
示例：跨平台 UI 组件
"""

from abc import ABC, abstractmethod


# Abstract Products
class Button(ABC):
    """Abstract product: Button."""
    @abstractmethod
    def render(self) -> str:
        pass

    @abstractmethod
    def on_click(self, callback: str) -> str:
        pass


class Checkbox(ABC):
    """Abstract product: Checkbox."""
    @abstractmethod
    def render(self) -> str:
        pass

    @abstractmethod
    def toggle(self) -> str:
        pass


class TextField(ABC):
    """Abstract product: TextField."""
    @abstractmethod
    def render(self) -> str:
        pass

    @abstractmethod
    def get_value(self) -> str:
        pass


# Concrete Products - Windows Family
class WindowsButton(Button):
    def render(self) -> str:
        return "[====Windows Button====]"

    def on_click(self, callback: str) -> str:
        return f"Windows button clicked: {callback}"


class WindowsCheckbox(Checkbox):
    def __init__(self):
        self.checked = False

    def render(self) -> str:
        mark = "☑" if self.checked else "☐"
        return f"[{mark}] Windows Checkbox"

    def toggle(self) -> str:
        self.checked = not self.checked
        return f"Windows checkbox toggled to {self.checked}"


class WindowsTextField(TextField):
    def __init__(self):
        self.value = ""

    def render(self) -> str:
        return f"|_____{self.value}_____| (Windows)"

    def get_value(self) -> str:
        return self.value


# Concrete Products - macOS Family
class MacOSButton(Button):
    def render(self) -> str:
        return "(  macOS Button  )"

    def on_click(self, callback: str) -> str:
        return f"macOS button clicked: {callback}"


class MacOSCheckbox(Checkbox):
    def __init__(self):
        self.checked = False

    def render(self) -> str:
        mark = "●" if self.checked else "○"
        return f"({mark}) macOS Checkbox"

    def toggle(self) -> str:
        self.checked = not self.checked
        return f"macOS checkbox toggled to {self.checked}"


class MacOSTextField(TextField):
    def __init__(self):
        self.value = ""

    def render(self) -> str:
        return f"[     {self.value}     ] (macOS)"

    def get_value(self) -> str:
        return self.value


# Abstract Factory
class GUIFactory(ABC):
    """
    Abstract factory interface.
    抽象工厂接口：声明创建产品族的方法。
    """
    @abstractmethod
    def create_button(self) -> Button:
        pass

    @abstractmethod
    def create_checkbox(self) -> Checkbox:
        pass

    @abstractmethod
    def create_textfield(self) -> TextField:
        pass


# Concrete Factories
class WindowsFactory(GUIFactory):
    """
    Concrete factory for Windows UI components.
    Windows UI 组件的具体工厂。
    """
    def create_button(self) -> Button:
        return WindowsButton()

    def create_checkbox(self) -> Checkbox:
        return WindowsCheckbox()

    def create_textfield(self) -> TextField:
        return WindowsTextField()


class MacOSFactory(GUIFactory):
    """
    Concrete factory for macOS UI components.
    macOS UI 组件的具体工厂。
    """
    def create_button(self) -> Button:
        return MacOSButton()

    def create_checkbox(self) -> Checkbox:
        return MacOSCheckbox()

    def create_textfield(self) -> TextField:
        return MacOSTextField()


# Application that uses the factory
class Application:
    """
    Application that uses abstract factory to create UI.
    使用抽象工厂创建 UI 的应用程序。
    """
    def __init__(self, factory: GUIFactory):
        self.factory = factory
        self.button = factory.create_button()
        self.checkbox = factory.create_checkbox()
        self.textfield = factory.create_textfield()

    def render_ui(self) -> str:
        result = ["=== Application UI ==="]
        result.append(f"Button: {self.button.render()}")
        result.append(f"Checkbox: {self.checkbox.render()}")
        result.append(f"TextField: {self.textfield.render()}")
        return "\n".join(result)

    def interact(self) -> str:
        result = ["=== User Interaction ==="]
        result.append(self.button.on_click("submit_form"))
        result.append(self.checkbox.toggle())
        return "\n".join(result)


def get_factory(os_type: str) -> GUIFactory:
    """
    Factory function to select the appropriate factory.
    根据操作系统选择适当的工厂。
    """
    factories = {
        "windows": WindowsFactory,
        "macos": MacOSFactory,
    }
    factory_class = factories.get(os_type.lower())
    if factory_class is None:
        raise ValueError(f"Unknown OS type: {os_type}")
    return factory_class()


# Client code demonstration
if __name__ == "__main__":
    print("=== Abstract Factory Pattern Demo ===\n")

    for os_type in ["Windows", "macOS"]:
        print(f"--- {os_type} Application ---\n")
        factory = get_factory(os_type)
        app = Application(factory)
        print(app.render_ui())
        print()
        print(app.interact())
        print("\n")
```

---

## 5. Builder Pattern (生成器模式)

**Separate the construction of a complex object from its representation, allowing the same construction process to create different representations.**

### 中文详解

生成器模式（也称建造者模式）是一种创建型设计模式，它将一个复杂对象的构建与它的表示分离，使得同样的构建过程可以创建不同的表示。

**适用场景：**
- 当创建复杂对象的算法应该独立于该对象的组成部分以及它们的装配方式时
- 当构造过程必须允许被构造的对象有不同的表示时
- 当需要生成的对象有复杂的内部结构（多个成员属性）时
- 例如：构建复杂的文档、餐厅点餐系统、电脑配置、SQL 查询构建器

**优点：**
- 可以分步创建对象，暂缓创建步骤或递归运行创建步骤
- 生成不同形式的产品时可以复用相同的制造代码
- 单一职责原则：将复杂构造代码从产品的业务逻辑中分离出来

**缺点：**
- 由于需要新增多个类，代码整体复杂程度会有所增加

### Structure Diagram

```
+-------------------+        +-------------------+
|     Director      |        |     Builder       |
+-------------------+        |   <<interface>>   |
| - builder: Builder|------->+-------------------+
+-------------------+        | + build_part_a()  |
| + construct()     |        | + build_part_b()  |
+-------------------+        | + build_part_c()  |
                             | + get_result()    |
                             +-------------------+
                                      ^
                                      |
                   +------------------+------------------+
                   |                                     |
         +-------------------+                 +-------------------+
         | ConcreteBuilder1  |                 | ConcreteBuilder2  |
         +-------------------+                 +-------------------+
         | - product: Product|                 | - product: Product|
         +-------------------+                 +-------------------+
         | + build_part_a()  |                 | + build_part_a()  |
         | + build_part_b()  |                 | + build_part_b()  |
         | + get_result()    |                 | + get_result()    |
         +-------------------+                 +-------------------+
                   |                                     |
                   v                                     v
         +-------------------+                 +-------------------+
         |     Product1      |                 |     Product2      |
         +-------------------+                 +-------------------+
```

**图解说明：**
- `Director` 指导者，负责控制构建过程的顺序
- `Builder` 抽象生成器接口，定义构建步骤
- `ConcreteBuilder` 具体生成器，实现构建步骤和获取结果
- `Product` 最终产品，由生成器逐步构建
- Director 不直接创建产品，而是通过 Builder 接口构建

### Python Code Example

```python
"""
Builder Pattern Implementation in Python
生成器模式的 Python 实现

Example: Building computers with different configurations
示例：构建不同配置的电脑
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class Computer:
    """
    Product class - the complex object being built.
    产品类 - 被构建的复杂对象。
    """
    cpu: str = ""
    ram: str = ""
    storage: str = ""
    gpu: str = ""
    os: str = ""
    peripherals: List[str] = field(default_factory=list)

    def __str__(self) -> str:
        parts = [
            f"CPU: {self.cpu}",
            f"RAM: {self.ram}",
            f"Storage: {self.storage}",
            f"GPU: {self.gpu}",
            f"OS: {self.os}",
            f"Peripherals: {', '.join(self.peripherals) if self.peripherals else 'None'}",
        ]
        return "\n".join(parts)


class ComputerBuilder(ABC):
    """
    Abstract builder interface.
    抽象生成器接口。
    """
    @abstractmethod
    def reset(self) -> None:
        pass

    @abstractmethod
    def set_cpu(self) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def set_ram(self) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def set_storage(self) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def set_gpu(self) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def set_os(self) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def add_peripheral(self, peripheral: str) -> 'ComputerBuilder':
        pass

    @abstractmethod
    def get_result(self) -> Computer:
        pass


class GamingPCBuilder(ComputerBuilder):
    """
    Concrete builder for gaming PCs.
    游戏电脑的具体生成器。
    """
    def __init__(self):
        self._computer: Optional[Computer] = None
        self.reset()

    def reset(self) -> None:
        self._computer = Computer()

    def set_cpu(self) -> 'GamingPCBuilder':
        self._computer.cpu = "Intel Core i9-13900K"
        return self

    def set_ram(self) -> 'GamingPCBuilder':
        self._computer.ram = "64GB DDR5 5600MHz"
        return self

    def set_storage(self) -> 'GamingPCBuilder':
        self._computer.storage = "2TB NVMe SSD + 4TB HDD"
        return self

    def set_gpu(self) -> 'GamingPCBuilder':
        self._computer.gpu = "NVIDIA RTX 4090"
        return self

    def set_os(self) -> 'GamingPCBuilder':
        self._computer.os = "Windows 11 Pro"
        return self

    def add_peripheral(self, peripheral: str) -> 'GamingPCBuilder':
        self._computer.peripherals.append(peripheral)
        return self

    def get_result(self) -> Computer:
        result = self._computer
        self.reset()  # Ready for next build
        return result


class OfficePCBuilder(ComputerBuilder):
    """
    Concrete builder for office PCs.
    办公电脑的具体生成器。
    """
    def __init__(self):
        self._computer: Optional[Computer] = None
        self.reset()

    def reset(self) -> None:
        self._computer = Computer()

    def set_cpu(self) -> 'OfficePCBuilder':
        self._computer.cpu = "Intel Core i5-13400"
        return self

    def set_ram(self) -> 'OfficePCBuilder':
        self._computer.ram = "16GB DDR4 3200MHz"
        return self

    def set_storage(self) -> 'OfficePCBuilder':
        self._computer.storage = "512GB NVMe SSD"
        return self

    def set_gpu(self) -> 'OfficePCBuilder':
        self._computer.gpu = "Intel UHD Graphics 730"
        return self

    def set_os(self) -> 'OfficePCBuilder':
        self._computer.os = "Windows 11 Pro"
        return self

    def add_peripheral(self, peripheral: str) -> 'OfficePCBuilder':
        self._computer.peripherals.append(peripheral)
        return self

    def get_result(self) -> Computer:
        result = self._computer
        self.reset()
        return result


class Director:
    """
    Director class - controls the building process.
    指导者类 - 控制构建过程。
    """
    def __init__(self):
        self._builder: Optional[ComputerBuilder] = None

    @property
    def builder(self) -> ComputerBuilder:
        return self._builder

    @builder.setter
    def builder(self, builder: ComputerBuilder) -> None:
        self._builder = builder

    def build_minimal_pc(self) -> Computer:
        """Build a minimal configuration PC."""
        return (self._builder
                .set_cpu()
                .set_ram()
                .set_storage()
                .get_result())

    def build_full_featured_pc(self) -> Computer:
        """Build a fully configured PC."""
        return (self._builder
                .set_cpu()
                .set_ram()
                .set_storage()
                .set_gpu()
                .set_os()
                .add_peripheral("Mechanical Keyboard")
                .add_peripheral("Gaming Mouse")
                .add_peripheral("27\" Monitor")
                .get_result())


# Fluent Builder alternative (without Director)
class FluentComputerBuilder:
    """
    Fluent builder - allows method chaining without Director.
    流式生成器 - 允许方法链式调用，无需指导者。
    """
    def __init__(self):
        self._computer = Computer()

    def cpu(self, cpu: str) -> 'FluentComputerBuilder':
        self._computer.cpu = cpu
        return self

    def ram(self, ram: str) -> 'FluentComputerBuilder':
        self._computer.ram = ram
        return self

    def storage(self, storage: str) -> 'FluentComputerBuilder':
        self._computer.storage = storage
        return self

    def gpu(self, gpu: str) -> 'FluentComputerBuilder':
        self._computer.gpu = gpu
        return self

    def os(self, os: str) -> 'FluentComputerBuilder':
        self._computer.os = os
        return self

    def peripheral(self, peripheral: str) -> 'FluentComputerBuilder':
        self._computer.peripherals.append(peripheral)
        return self

    def build(self) -> Computer:
        return self._computer


# Client code demonstration
if __name__ == "__main__":
    print("=== Builder Pattern Demo ===\n")

    # Using Director with different builders
    director = Director()

    print("1. Gaming PC (Full Configuration):")
    director.builder = GamingPCBuilder()
    gaming_pc = director.build_full_featured_pc()
    print(gaming_pc)
    print()

    print("2. Office PC (Minimal Configuration):")
    director.builder = OfficePCBuilder()
    office_pc = director.build_minimal_pc()
    print(office_pc)
    print()

    # Using Fluent Builder directly (without Director)
    print("3. Custom PC (Fluent Builder):")
    custom_pc = (FluentComputerBuilder()
                 .cpu("AMD Ryzen 9 7950X")
                 .ram("32GB DDR5")
                 .storage("1TB NVMe SSD")
                 .gpu("AMD RX 7900 XTX")
                 .os("Ubuntu 22.04 LTS")
                 .peripheral("Ergonomic Keyboard")
                 .build())
    print(custom_pc)
```

---

## 6. Prototype Pattern (原型模式)

**Specify the kinds of objects to create using a prototypical instance, and create new objects by copying this prototype.**

### 中文详解

原型模式是一种创建型设计模式，它允许通过复制现有对象来创建新对象，而无需使代码依赖于它们所属的类。

**适用场景：**
- 当需要创建的对象类型由实例确定时
- 当创建对象的代价比较大（如需要从数据库获取数据）时
- 当系统应该独立于产品的创建、构成和表示时
- 当需要避免创建一个与产品类层次平行的工厂类层次时
- 例如：复制文档、克隆游戏对象、配置模板

**优点：**
- 可以克隆对象而无需与它们所属的具体类耦合
- 可以克隆预生成原型，避免反复运行初始化代码
- 可以更方便地生成复杂对象
- 可以用继承以外的方式处理复杂对象的不同配置

**缺点：**
- 克隆包含循环引用的复杂对象可能非常麻烦

### Structure Diagram

```
+-------------------------+
|       Prototype         |
|     <<interface>>       |
+-------------------------+
| + clone(): Prototype    |
+-------------------------+
            ^
            |
     +------+------+
     |             |
+----------+  +----------+
|ConcreteA |  |ConcreteB |
+----------+  +----------+
| - field1 |  | - fieldX |
| - field2 |  | - fieldY |
+----------+  +----------+
| + clone()|  | + clone()|
+----------+  +----------+

        Registry (optional)
+--------------------------------+
|       PrototypeRegistry        |
+--------------------------------+
| - prototypes: Dict[str, Proto] |
+--------------------------------+
| + register(key, prototype)     |
| + get_prototype(key): Proto    |
+--------------------------------+
```

**图解说明：**
- `Prototype` 接口声明克隆方法
- `ConcretePrototype` 实现克隆方法，复制自身
- `PrototypeRegistry` 可选的原型注册表，存储预配置的原型
- 客户端调用 `clone()` 获取新对象，无需知道具体类

### Python Code Example

```python
"""
Prototype Pattern Implementation in Python
原型模式的 Python 实现

Example: Document templates and game characters
示例：文档模板和游戏角色
"""

import copy
from abc import ABC, abstractmethod
from typing import Dict, Any, List


class Prototype(ABC):
    """
    Prototype interface.
    原型接口。
    """
    @abstractmethod
    def clone(self) -> 'Prototype':
        """Create a copy of this object."""
        pass


class Document(Prototype):
    """
    Concrete prototype: Document.
    具体原型：文档。
    """
    def __init__(self, title: str, content: str, 
                 author: str, styles: Dict[str, Any] = None):
        self.title = title
        self.content = content
        self.author = author
        self.styles = styles or {}
        self.sections: List[str] = []

    def add_section(self, section: str) -> None:
        self.sections.append(section)

    def clone(self) -> 'Document':
        """
        Deep copy the document.
        深拷贝文档。
        """
        # Use copy.deepcopy for nested objects
        cloned = copy.deepcopy(self)
        return cloned

    def __str__(self) -> str:
        return (f"Document(title='{self.title}', author='{self.author}', "
                f"sections={len(self.sections)}, styles={self.styles})")


class GameCharacter(Prototype):
    """
    Concrete prototype: Game Character with complex nested state.
    具体原型：具有复杂嵌套状态的游戏角色。
    """
    def __init__(self, name: str, character_class: str):
        self.name = name
        self.character_class = character_class
        self.level = 1
        self.health = 100
        self.stats = {
            "strength": 10,
            "agility": 10,
            "intelligence": 10,
        }
        self.inventory: List[str] = []
        self.skills: List[str] = []

    def add_item(self, item: str) -> None:
        self.inventory.append(item)

    def learn_skill(self, skill: str) -> None:
        self.skills.append(skill)

    def level_up(self) -> None:
        self.level += 1
        self.health += 10
        for stat in self.stats:
            self.stats[stat] += 2

    def clone(self) -> 'GameCharacter':
        """
        Deep copy the character.
        Note: In games, you often want shallow copy for some attributes.
        深拷贝角色。注意：游戏中有时需要浅拷贝某些属性。
        """
        return copy.deepcopy(self)

    def __str__(self) -> str:
        return (f"Character(name='{self.name}', class='{self.character_class}', "
                f"level={self.level}, health={self.health}, "
                f"items={len(self.inventory)}, skills={self.skills})")


class PrototypeRegistry:
    """
    Prototype registry - stores pre-configured prototypes.
    原型注册表 - 存储预配置的原型。
    """
    def __init__(self):
        self._prototypes: Dict[str, Prototype] = {}

    def register(self, key: str, prototype: Prototype) -> None:
        """Register a prototype with a key."""
        self._prototypes[key] = prototype

    def unregister(self, key: str) -> None:
        """Remove a prototype from registry."""
        del self._prototypes[key]

    def get(self, key: str) -> Prototype:
        """Get a clone of the registered prototype."""
        prototype = self._prototypes.get(key)
        if prototype is None:
            raise KeyError(f"Prototype '{key}' not found")
        return prototype.clone()

    def list_prototypes(self) -> List[str]:
        """List all registered prototype keys."""
        return list(self._prototypes.keys())


# Client code demonstration
if __name__ == "__main__":
    print("=== Prototype Pattern Demo ===\n")

    # Example 1: Document cloning
    print("1. Document Cloning:")
    
    # Create a template document
    template = Document(
        title="Annual Report Template",
        content="Executive Summary...",
        author="Template Author",
        styles={"font": "Arial", "size": 12, "margin": 1}
    )
    template.add_section("Introduction")
    template.add_section("Financial Results")
    template.add_section("Conclusion")
    
    print(f"   Original: {template}")
    
    # Clone and customize
    report_2023 = template.clone()
    report_2023.title = "Annual Report 2023"
    report_2023.author = "John Smith"
    report_2023.add_section("2023 Highlights")
    
    print(f"   Clone 1:  {report_2023}")
    
    report_2024 = template.clone()
    report_2024.title = "Annual Report 2024"
    report_2024.author = "Jane Doe"
    
    print(f"   Clone 2:  {report_2024}")
    print(f"   Original unchanged: {template}")
    print()

    # Example 2: Game character cloning with registry
    print("2. Game Character Cloning with Registry:")
    
    # Create prototype characters
    warrior_proto = GameCharacter("Warrior Template", "Warrior")
    warrior_proto.stats["strength"] = 15
    warrior_proto.stats["agility"] = 8
    warrior_proto.learn_skill("Slash")
    warrior_proto.learn_skill("Shield Block")
    warrior_proto.add_item("Iron Sword")
    warrior_proto.add_item("Wooden Shield")
    
    mage_proto = GameCharacter("Mage Template", "Mage")
    mage_proto.stats["intelligence"] = 15
    mage_proto.stats["strength"] = 6
    mage_proto.learn_skill("Fireball")
    mage_proto.learn_skill("Ice Shield")
    mage_proto.add_item("Staff")
    mage_proto.add_item("Spellbook")
    
    # Register prototypes
    registry = PrototypeRegistry()
    registry.register("warrior", warrior_proto)
    registry.register("mage", mage_proto)
    
    print(f"   Registered prototypes: {registry.list_prototypes()}")
    
    # Create characters from prototypes
    player1 = registry.get("warrior")
    player1.name = "Conan"
    player1.level_up()
    player1.add_item("Health Potion")
    
    player2 = registry.get("mage")
    player2.name = "Gandalf"
    player2.level_up()
    player2.level_up()
    player2.learn_skill("Teleport")
    
    player3 = registry.get("warrior")
    player3.name = "Aragorn"
    
    print(f"   Player 1: {player1}")
    print(f"   Player 2: {player2}")
    print(f"   Player 3: {player3}")
    print(f"   Original warrior unchanged: {warrior_proto}")
```

---

## Summary Table

| Pattern | Purpose | Key Benefit |
|---------|---------|-------------|
| **Singleton** | One instance only | Global access point |
| **Simple Factory** | Centralized creation logic | Decouples client from concrete classes |
| **Factory Method** | Subclass decides what to create | Open for extension |
| **Abstract Factory** | Create families of objects | Ensures consistency |
| **Builder** | Step-by-step construction | Complex object creation |
| **Prototype** | Clone existing objects | Avoid costly initialization |

---

*Next: See `design_patterns_structural.md` for structural patterns.*

