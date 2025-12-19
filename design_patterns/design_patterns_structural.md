# Design Patterns - Structural Patterns (结构型模式)

A comprehensive guide to structural design patterns with English explanations,
Chinese details, ASCII diagrams, and Python code examples.

---

## Table of Contents

1. [Adapter Pattern (适配器模式)](#1-adapter-pattern-适配器模式)
2. [Bridge Pattern (桥接模式)](#2-bridge-pattern-桥接模式)
3. [Composite Pattern (组合模式)](#3-composite-pattern-组合模式)
4. [Decorator Pattern (装饰器模式)](#4-decorator-pattern-装饰器模式)
5. [Facade Pattern (外观模式)](#5-facade-pattern-外观模式)
6. [Flyweight Pattern (享元模式)](#6-flyweight-pattern-享元模式)
7. [Proxy Pattern (代理模式)](#7-proxy-pattern-代理模式)

---

## 1. Adapter Pattern (适配器模式)

**Convert the interface of a class into another interface clients expect, allowing classes with incompatible interfaces to work together.**

### 中文详解

适配器模式是一种结构型设计模式，它允许将一个类的接口转换成客户希望的另外一个接口。适配器模式使得原本由于接口不兼容而不能一起工作的那些类可以一起工作。

**适用场景：**
- 当你希望使用某个类，但其接口与其他代码不兼容时
- 当你需要复用多个现有的子类，但它们缺少共同的功能且无法扩展超类时
- 例如：集成第三方库、旧系统升级、数据格式转换

**两种实现方式：**
- 对象适配器：使用组合，适配器包含被适配者的实例
- 类适配器：使用多重继承（Python 支持）

**优点：**
- 单一职责原则：将接口转换代码与业务逻辑分离
- 开闭原则：可以引入新适配器而无需修改现有代码

**缺点：**
- 代码整体复杂度增加，需要引入新接口和类

### Structure Diagram

```
Object Adapter:
                                                  
+-------------+      +--------------+      +----------------+
|   Client    |----->|    Target    |      |    Adaptee     |
+-------------+      |  <<interface>>|      +----------------+
                     +--------------+      | + specific_    |
                     | + request()  |      |   request()    |
                     +--------------+      +----------------+
                            ^                      ^
                            |                      |
                     +--------------+              |
                     |   Adapter    |--------------+
                     +--------------+   (contains)
                     | - adaptee    |
                     +--------------+
                     | + request()  |
                     +--------------+

Class Adapter (multiple inheritance):

+-------------+      +--------------+      +----------------+
|   Client    |----->|    Target    |      |    Adaptee     |
+-------------+      |  <<interface>>|      +----------------+
                     +--------------+      | + specific_    |
                     | + request()  |      |   request()    |
                     +--------------+      +----------------+
                            ^                      ^
                            |                      |
                            +-------+------+-------+
                                    |
                             +--------------+
                             |   Adapter    |
                             +--------------+
                             | + request()  |
                             +--------------+
```

**图解说明：**
- `Target` 是客户端期望的接口
- `Adaptee` 是需要适配的现有类，接口不兼容
- `Adapter` 适配器，将 Adaptee 的接口转换为 Target 接口
- 对象适配器通过组合持有 Adaptee 实例
- 类适配器通过多重继承同时继承 Target 和 Adaptee

### Python Code Example

```python
"""
Adapter Pattern Implementation in Python
适配器模式的 Python 实现

Example: Integrating different payment systems
示例：集成不同的支付系统
"""

from abc import ABC, abstractmethod
from typing import Dict, Any
import json


# Target interface - what our system expects
class PaymentProcessor(ABC):
    """
    Target interface for payment processing.
    支付处理的目标接口。
    """
    @abstractmethod
    def pay(self, amount: float, currency: str = "USD") -> Dict[str, Any]:
        """Process a payment."""
        pass

    @abstractmethod
    def refund(self, transaction_id: str, amount: float) -> bool:
        """Process a refund."""
        pass


# Adaptee 1 - Legacy payment system with different interface
class LegacyPaymentGateway:
    """
    Legacy payment system with incompatible interface.
    具有不兼容接口的旧版支付系统。
    """
    def process_payment(self, cents: int, currency_code: str) -> str:
        """Process payment in cents, returns XML response."""
        return f"""<payment>
            <status>success</status>
            <transaction_id>LEGACY-{cents}-{currency_code}</transaction_id>
            <amount_cents>{cents}</amount_cents>
        </payment>"""

    def cancel_transaction(self, txn_id: str, cents: int) -> str:
        """Cancel a transaction, returns XML response."""
        return f"""<refund>
            <status>refunded</status>
            <original_transaction>{txn_id}</original_transaction>
        </refund>"""


# Adaptee 2 - Third-party payment API
class ThirdPartyPaymentAPI:
    """
    Third-party payment API with different interface.
    第三方支付 API，接口不同。
    """
    def make_payment(self, payment_data: Dict) -> Dict:
        """Process payment with dictionary input/output."""
        return {
            "success": True,
            "id": f"3RD-{payment_data['value']}-{payment_data['curr']}",
            "amount": payment_data["value"],
            "currency": payment_data["curr"]
        }

    def reverse_payment(self, payment_id: str, reverse_amount: float) -> Dict:
        """Reverse a payment."""
        return {
            "reversed": True,
            "payment_id": payment_id,
            "refunded_amount": reverse_amount
        }


# Object Adapter for Legacy System
class LegacyPaymentAdapter(PaymentProcessor):
    """
    Adapter for the legacy payment gateway.
    旧版支付网关的适配器。
    """
    def __init__(self, legacy_gateway: LegacyPaymentGateway):
        self._legacy = legacy_gateway

    def pay(self, amount: float, currency: str = "USD") -> Dict[str, Any]:
        """Convert dollars to cents and parse XML response."""
        cents = int(amount * 100)
        xml_response = self._legacy.process_payment(cents, currency)
        
        # Parse XML to dict (simplified)
        import re
        txn_id = re.search(r'<transaction_id>(.+?)</transaction_id>', 
                          xml_response).group(1)
        
        return {
            "success": True,
            "transaction_id": txn_id,
            "amount": amount,
            "currency": currency,
            "adapter": "LegacyPaymentAdapter"
        }

    def refund(self, transaction_id: str, amount: float) -> bool:
        """Convert and process refund."""
        cents = int(amount * 100)
        xml_response = self._legacy.cancel_transaction(transaction_id, cents)
        return "refunded" in xml_response


# Object Adapter for Third-Party API
class ThirdPartyPaymentAdapter(PaymentProcessor):
    """
    Adapter for the third-party payment API.
    第三方支付 API 的适配器。
    """
    def __init__(self, api: ThirdPartyPaymentAPI):
        self._api = api

    def pay(self, amount: float, currency: str = "USD") -> Dict[str, Any]:
        """Convert to third-party format."""
        payment_data = {
            "value": amount,
            "curr": currency
        }
        response = self._api.make_payment(payment_data)
        
        return {
            "success": response["success"],
            "transaction_id": response["id"],
            "amount": response["amount"],
            "currency": response["currency"],
            "adapter": "ThirdPartyPaymentAdapter"
        }

    def refund(self, transaction_id: str, amount: float) -> bool:
        """Process refund through third-party API."""
        response = self._api.reverse_payment(transaction_id, amount)
        return response.get("reversed", False)


# Client code that works with any PaymentProcessor
class PaymentService:
    """
    Client that uses the PaymentProcessor interface.
    使用 PaymentProcessor 接口的客户端。
    """
    def __init__(self, processor: PaymentProcessor):
        self._processor = processor

    def checkout(self, amount: float, currency: str = "USD") -> Dict[str, Any]:
        """Process a checkout."""
        print(f"Processing payment of {amount} {currency}...")
        result = self._processor.pay(amount, currency)
        print(f"Payment result: {json.dumps(result, indent=2)}")
        return result

    def process_refund(self, transaction_id: str, amount: float) -> bool:
        """Process a refund."""
        print(f"Processing refund of {amount} for {transaction_id}...")
        success = self._processor.refund(transaction_id, amount)
        print(f"Refund {'successful' if success else 'failed'}")
        return success


# Client code demonstration
if __name__ == "__main__":
    print("=== Adapter Pattern Demo ===\n")

    # Using legacy payment system through adapter
    print("1. Using Legacy Payment System:")
    legacy_gateway = LegacyPaymentGateway()
    legacy_adapter = LegacyPaymentAdapter(legacy_gateway)
    service1 = PaymentService(legacy_adapter)
    result1 = service1.checkout(99.99, "USD")
    service1.process_refund(result1["transaction_id"], 99.99)
    print()

    # Using third-party API through adapter
    print("2. Using Third-Party Payment API:")
    third_party_api = ThirdPartyPaymentAPI()
    third_party_adapter = ThirdPartyPaymentAdapter(third_party_api)
    service2 = PaymentService(third_party_adapter)
    result2 = service2.checkout(149.99, "EUR")
    service2.process_refund(result2["transaction_id"], 149.99)
```

---

## 2. Bridge Pattern (桥接模式)

**Decouple an abstraction from its implementation so that the two can vary independently.**

### 中文详解

桥接模式是一种结构型设计模式，它将抽象部分与实现部分分离，使它们都可以独立地变化。

**适用场景：**
- 当你想要避免抽象和实现之间的永久绑定时
- 当抽象和实现都应该可以通过子类化独立扩展时
- 当对一个抽象的实现进行修改不应影响客户端代码时
- 例如：跨平台 GUI（形状 × 渲染引擎）、设备驱动程序

**与适配器的区别：**
- 适配器：让已有类协同工作（事后补救）
- 桥接：预先设计，让抽象和实现独立演化

**优点：**
- 可以创建与平台无关的类和程序
- 客户端代码仅与高层抽象部分交互
- 开闭原则：可以独立扩展抽象和实现
- 单一职责原则：抽象关注高层逻辑，实现关注平台细节

**缺点：**
- 对高内聚的类使用该模式可能会让代码更加复杂

### Structure Diagram

```
+-------------------+                  +-------------------+
|    Abstraction    |                  |   Implementor     |
+-------------------+    has-a         |   <<interface>>   |
| - implementor ----|----------------->+-------------------+
+-------------------+                  | + operation_impl()|
| + operation()     |                  +-------------------+
+-------------------+                           ^
         ^                                      |
         |                         +------------+------------+
         |                         |                         |
+-------------------+    +-------------------+    +-------------------+
| RefinedAbstraction|    | ConcreteImplA     |    | ConcreteImplB     |
+-------------------+    +-------------------+    +-------------------+
| + operation()     |    | + operation_impl()|    | + operation_impl()|
| + extended_op()   |    +-------------------+    +-------------------+
+-------------------+

         Abstraction                    Implementation
        (WHAT to do)                   (HOW to do it)
```

**图解说明：**
- `Abstraction` 定义抽象部分的接口，持有实现的引用
- `RefinedAbstraction` 扩展抽象接口
- `Implementor` 定义实现部分的接口
- `ConcreteImpl` 具体实现
- 抽象和实现可以独立变化，通过组合而非继承连接

### Python Code Example

```python
"""
Bridge Pattern Implementation in Python
桥接模式的 Python 实现

Example: Remote controls (abstraction) and devices (implementation)
示例：遥控器（抽象）和设备（实现）
"""

from abc import ABC, abstractmethod


# Implementor interface
class Device(ABC):
    """
    Implementation interface for devices.
    设备的实现接口。
    """
    @abstractmethod
    def is_enabled(self) -> bool:
        pass

    @abstractmethod
    def enable(self) -> None:
        pass

    @abstractmethod
    def disable(self) -> None:
        pass

    @abstractmethod
    def get_volume(self) -> int:
        pass

    @abstractmethod
    def set_volume(self, volume: int) -> None:
        pass

    @abstractmethod
    def get_channel(self) -> int:
        pass

    @abstractmethod
    def set_channel(self, channel: int) -> None:
        pass

    @abstractmethod
    def get_device_info(self) -> str:
        pass


# Concrete Implementors
class TV(Device):
    """Concrete implementation: Television."""
    def __init__(self):
        self._on = False
        self._volume = 30
        self._channel = 1

    def is_enabled(self) -> bool:
        return self._on

    def enable(self) -> None:
        self._on = True

    def disable(self) -> None:
        self._on = False

    def get_volume(self) -> int:
        return self._volume

    def set_volume(self, volume: int) -> None:
        self._volume = max(0, min(100, volume))

    def get_channel(self) -> int:
        return self._channel

    def set_channel(self, channel: int) -> None:
        self._channel = max(1, channel)

    def get_device_info(self) -> str:
        status = "ON" if self._on else "OFF"
        return f"TV [{status}] Vol:{self._volume} Ch:{self._channel}"


class Radio(Device):
    """Concrete implementation: Radio."""
    def __init__(self):
        self._on = False
        self._volume = 20
        self._frequency = 87.5  # FM frequency

    def is_enabled(self) -> bool:
        return self._on

    def enable(self) -> None:
        self._on = True

    def disable(self) -> None:
        self._on = False

    def get_volume(self) -> int:
        return self._volume

    def set_volume(self, volume: int) -> None:
        self._volume = max(0, min(100, volume))

    def get_channel(self) -> int:
        # Convert frequency to "channel" number
        return int(self._frequency * 10)

    def set_channel(self, channel: int) -> None:
        # Convert channel to frequency
        self._frequency = channel / 10.0

    def get_device_info(self) -> str:
        status = "ON" if self._on else "OFF"
        return f"Radio [{status}] Vol:{self._volume} Freq:{self._frequency}MHz"


class SmartSpeaker(Device):
    """Concrete implementation: Smart Speaker."""
    def __init__(self):
        self._on = False
        self._volume = 50
        self._playlist = 1

    def is_enabled(self) -> bool:
        return self._on

    def enable(self) -> None:
        self._on = True
        print("  [Smart Speaker: 'Hello! How can I help you?']")

    def disable(self) -> None:
        self._on = False
        print("  [Smart Speaker: 'Goodbye!']")

    def get_volume(self) -> int:
        return self._volume

    def set_volume(self, volume: int) -> None:
        self._volume = max(0, min(100, volume))

    def get_channel(self) -> int:
        return self._playlist

    def set_channel(self, channel: int) -> None:
        self._playlist = max(1, channel)
        print(f"  [Smart Speaker: Playing playlist {self._playlist}]")

    def get_device_info(self) -> str:
        status = "ON" if self._on else "OFF"
        return f"SmartSpeaker [{status}] Vol:{self._volume} Playlist:{self._playlist}"


# Abstraction
class RemoteControl:
    """
    Abstraction: Basic remote control.
    抽象：基本遥控器。
    """
    def __init__(self, device: Device):
        self._device = device

    def toggle_power(self) -> None:
        """Toggle device power."""
        if self._device.is_enabled():
            self._device.disable()
            print(f"  Power OFF: {self._device.get_device_info()}")
        else:
            self._device.enable()
            print(f"  Power ON: {self._device.get_device_info()}")

    def volume_up(self) -> None:
        """Increase volume."""
        self._device.set_volume(self._device.get_volume() + 10)
        print(f"  Volume UP: {self._device.get_device_info()}")

    def volume_down(self) -> None:
        """Decrease volume."""
        self._device.set_volume(self._device.get_volume() - 10)
        print(f"  Volume DOWN: {self._device.get_device_info()}")

    def channel_up(self) -> None:
        """Next channel."""
        self._device.set_channel(self._device.get_channel() + 1)
        print(f"  Channel UP: {self._device.get_device_info()}")

    def channel_down(self) -> None:
        """Previous channel."""
        self._device.set_channel(self._device.get_channel() - 1)
        print(f"  Channel DOWN: {self._device.get_device_info()}")


# Refined Abstraction
class AdvancedRemoteControl(RemoteControl):
    """
    Refined abstraction: Advanced remote with extra features.
    扩展抽象：具有额外功能的高级遥控器。
    """
    def mute(self) -> None:
        """Mute the device."""
        self._device.set_volume(0)
        print(f"  MUTE: {self._device.get_device_info()}")

    def set_channel_direct(self, channel: int) -> None:
        """Set channel directly."""
        self._device.set_channel(channel)
        print(f"  Channel SET to {channel}: {self._device.get_device_info()}")

    def print_status(self) -> None:
        """Print current device status."""
        print(f"  Status: {self._device.get_device_info()}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Bridge Pattern Demo ===\n")

    # Basic remote with TV
    print("1. Basic Remote Control with TV:")
    tv = TV()
    basic_remote = RemoteControl(tv)
    basic_remote.toggle_power()
    basic_remote.volume_up()
    basic_remote.channel_up()
    basic_remote.channel_up()
    print()

    # Advanced remote with Radio
    print("2. Advanced Remote Control with Radio:")
    radio = Radio()
    advanced_remote = AdvancedRemoteControl(radio)
    advanced_remote.toggle_power()
    advanced_remote.set_channel_direct(1015)  # 101.5 MHz
    advanced_remote.volume_up()
    advanced_remote.mute()
    advanced_remote.print_status()
    print()

    # Advanced remote with Smart Speaker
    print("3. Advanced Remote Control with Smart Speaker:")
    speaker = SmartSpeaker()
    speaker_remote = AdvancedRemoteControl(speaker)
    speaker_remote.toggle_power()
    speaker_remote.set_channel_direct(5)  # Playlist 5
    speaker_remote.volume_down()
    speaker_remote.toggle_power()
```

---

## 3. Composite Pattern (组合模式)

**Compose objects into tree structures to represent part-whole hierarchies, allowing clients to treat individual objects and compositions uniformly.**

### 中文详解

组合模式是一种结构型设计模式，它将对象组合成树形结构以表示"部分-整体"的层次结构。组合模式使得用户对单个对象和组合对象的使用具有一致性。

**适用场景：**
- 当你想表示对象的部分-整体层次结构时
- 当你希望用户忽略组合对象与单个对象的不同，统一地使用组合结构中的所有对象时
- 例如：文件系统（文件和文件夹）、GUI 组件树、组织结构图、菜单系统

**优点：**
- 可以利用多态和递归机制更方便地使用复杂树结构
- 开闭原则：无需更改现有代码，就可以在应用中添加新元素

**缺点：**
- 对于功能差异较大的类，提供公共接口可能会有困难

### Structure Diagram

```
+-------------------+
|     Component     |
|   <<interface>>   |
+-------------------+
| + operation()     |
| + add(Component)  |
| + remove(Component)
| + get_child(i)    |
+-------------------+
         ^
         |
    +----+----+
    |         |
+-------+  +-------------+
| Leaf  |  |  Composite  |
+-------+  +-------------+
|       |  | - children  |
+-------+  +-------------+
| + op()|  | + operation()| -----> for each child:
+-------+  | + add()     |          child.operation()
           | + remove()  |
           | + get_child()|
           +-------------+

Tree Structure Example:

              [Composite: Root]
               /      |      \
              /       |       \
      [Leaf]    [Composite]   [Leaf]
                  /     \
                 /       \
            [Leaf]     [Leaf]
```

**图解说明：**
- `Component` 定义所有对象的公共接口
- `Leaf` 叶节点，没有子节点
- `Composite` 容器节点，包含子节点
- 客户端通过 Component 接口操作所有对象
- Composite 的 operation() 递归调用子节点的 operation()

### Python Code Example

```python
"""
Composite Pattern Implementation in Python
组合模式的 Python 实现

Example: File system with files and directories
示例：包含文件和目录的文件系统
"""

from abc import ABC, abstractmethod
from typing import List


class FileSystemComponent(ABC):
    """
    Component interface for file system items.
    文件系统项目的组件接口。
    """
    def __init__(self, name: str):
        self._name = name
        self._parent: 'FileSystemComponent' = None

    @property
    def name(self) -> str:
        return self._name

    @property
    def parent(self) -> 'FileSystemComponent':
        return self._parent

    @parent.setter
    def parent(self, parent: 'FileSystemComponent'):
        self._parent = parent

    def get_path(self) -> str:
        """Get full path of this component."""
        if self._parent:
            return f"{self._parent.get_path()}/{self._name}"
        return self._name

    @abstractmethod
    def get_size(self) -> int:
        """Get size in bytes."""
        pass

    @abstractmethod
    def display(self, indent: int = 0) -> str:
        """Display the component structure."""
        pass

    def add(self, component: 'FileSystemComponent') -> None:
        """Add a child component (only for composites)."""
        raise NotImplementedError("Cannot add to a leaf component")

    def remove(self, component: 'FileSystemComponent') -> None:
        """Remove a child component (only for composites)."""
        raise NotImplementedError("Cannot remove from a leaf component")

    def is_composite(self) -> bool:
        """Check if this is a composite."""
        return False


class File(FileSystemComponent):
    """
    Leaf: Represents a file.
    叶节点：表示文件。
    """
    def __init__(self, name: str, size: int):
        super().__init__(name)
        self._size = size

    def get_size(self) -> int:
        return self._size

    def display(self, indent: int = 0) -> str:
        prefix = "  " * indent
        return f"{prefix}📄 {self._name} ({self._size} bytes)"


class Directory(FileSystemComponent):
    """
    Composite: Represents a directory containing other components.
    组合节点：表示包含其他组件的目录。
    """
    def __init__(self, name: str):
        super().__init__(name)
        self._children: List[FileSystemComponent] = []

    def add(self, component: FileSystemComponent) -> None:
        """Add a component to this directory."""
        self._children.append(component)
        component.parent = self

    def remove(self, component: FileSystemComponent) -> None:
        """Remove a component from this directory."""
        self._children.remove(component)
        component.parent = None

    def get_size(self) -> int:
        """Calculate total size of all children recursively."""
        return sum(child.get_size() for child in self._children)

    def display(self, indent: int = 0) -> str:
        """Display directory and all children."""
        prefix = "  " * indent
        result = [f"{prefix}📁 {self._name}/ ({self.get_size()} bytes)"]
        for child in self._children:
            result.append(child.display(indent + 1))
        return "\n".join(result)

    def is_composite(self) -> bool:
        return True

    def get_children(self) -> List[FileSystemComponent]:
        return self._children.copy()

    def find(self, name: str) -> FileSystemComponent:
        """Find a component by name recursively."""
        for child in self._children:
            if child.name == name:
                return child
            if child.is_composite():
                found = child.find(name)
                if found:
                    return found
        return None


class SymbolicLink(FileSystemComponent):
    """
    Leaf: Represents a symbolic link to another component.
    叶节点：表示指向另一个组件的符号链接。
    """
    def __init__(self, name: str, target: FileSystemComponent):
        super().__init__(name)
        self._target = target

    def get_size(self) -> int:
        # Symlink itself is small, but reports target size
        return self._target.get_size()

    def display(self, indent: int = 0) -> str:
        prefix = "  " * indent
        return f"{prefix}🔗 {self._name} -> {self._target.get_path()}"


# Client code demonstration
if __name__ == "__main__":
    print("=== Composite Pattern Demo ===\n")

    # Create file system structure
    root = Directory("root")

    # Create directories
    home = Directory("home")
    user = Directory("user")
    documents = Directory("documents")
    pictures = Directory("pictures")
    etc = Directory("etc")

    # Create files
    bashrc = File(".bashrc", 1024)
    profile = File(".profile", 512)
    readme = File("readme.txt", 2048)
    report = File("report.pdf", 102400)
    vacation = File("vacation.jpg", 2048000)
    profile_pic = File("profile.png", 512000)
    hosts = File("hosts", 256)
    passwd = File("passwd", 1024)

    # Build the tree structure
    root.add(home)
    root.add(etc)

    home.add(user)

    user.add(bashrc)
    user.add(profile)
    user.add(documents)
    user.add(pictures)

    documents.add(readme)
    documents.add(report)

    pictures.add(vacation)
    pictures.add(profile_pic)

    etc.add(hosts)
    etc.add(passwd)

    # Add a symbolic link
    link = SymbolicLink("docs_link", documents)
    home.add(link)

    # Display the entire file system
    print("1. Complete File System Structure:")
    print(root.display())
    print()

    # Calculate sizes at different levels
    print("2. Size Calculations:")
    print(f"   Total size of root: {root.get_size():,} bytes")
    print(f"   Size of documents: {documents.get_size():,} bytes")
    print(f"   Size of pictures: {pictures.get_size():,} bytes")
    print(f"   Size of single file (vacation.jpg): {vacation.get_size():,} bytes")
    print()

    # Find a component
    print("3. Finding Components:")
    found = root.find("report.pdf")
    if found:
        print(f"   Found: {found.get_path()}")
    
    found = root.find("pictures")
    if found:
        print(f"   Found: {found.get_path()}")
```

---

## 4. Decorator Pattern (装饰器模式)

**Attach additional responsibilities to an object dynamically, providing a flexible alternative to subclassing for extending functionality.**

### 中文详解

装饰器模式是一种结构型设计模式，它允许向一个现有的对象添加新的功能，同时又不改变其结构。装饰器模式是继承关系的一个替代方案。

**适用场景：**
- 在不影响其他对象的情况下，以动态、透明的方式给单个对象添加职责
- 当不能采用继承的方式对系统进行扩展或者采用继承不利于系统扩展和维护时
- 例如：Java I/O 流、咖啡加配料、文本格式化

**优点：**
- 比继承更灵活：可以在运行时添加或删除对象的职责
- 可以用多个装饰器包装对象，组合多种行为
- 单一职责原则：可以将特定行为分解到单独的类中

**缺点：**
- 在最终的装饰器栈中删除特定装饰器比较困难
- 装饰器的行为可能依赖于装饰器栈中的顺序

### Structure Diagram

```
+-------------------+
|     Component     |
|   <<interface>>   |
+-------------------+
| + operation()     |
+-------------------+
         ^
         |
    +----+----+
    |         |
+-------+  +-------------+
|Concrete|  |  Decorator  |
|Component| |  <<abstract>>|
+-------+  +-------------+
| + op()|  | - wrapped   |----> Component
+-------+  +-------------+
           | + operation()|
           +-------------+
                  ^
                  |
         +--------+--------+
         |                 |
+---------------+  +---------------+
| DecoratorA    |  | DecoratorB    |
+---------------+  +---------------+
| + operation() |  | + operation() |
| + extra_a()   |  | + extra_b()   |
+---------------+  +---------------+

Wrapping Example:

+----------------+
|  DecoratorB    |
|  +----------+  |
|  |DecoratorA|  |
|  | +------+ |  |
|  | |Concrt| |  |
|  | +------+ |  |
|  +----------+  |
+----------------+
```

**图解说明：**
- `Component` 定义对象接口
- `ConcreteComponent` 具体组件，被装饰的对象
- `Decorator` 抽象装饰器，持有 Component 引用
- `ConcreteDecorator` 具体装饰器，添加额外功能
- 装饰器可以嵌套，形成装饰器栈

### Python Code Example

```python
"""
Decorator Pattern Implementation in Python
装饰器模式的 Python 实现

Example: Coffee shop with customizable beverages
示例：可定制饮品的咖啡店
"""

from abc import ABC, abstractmethod


class Beverage(ABC):
    """
    Component interface for beverages.
    饮品的组件接口。
    """
    @abstractmethod
    def get_description(self) -> str:
        pass

    @abstractmethod
    def get_cost(self) -> float:
        pass


# Concrete Components
class Espresso(Beverage):
    """Concrete component: Espresso."""
    def get_description(self) -> str:
        return "Espresso"

    def get_cost(self) -> float:
        return 1.99


class HouseBlend(Beverage):
    """Concrete component: House Blend Coffee."""
    def get_description(self) -> str:
        return "House Blend Coffee"

    def get_cost(self) -> float:
        return 0.89


class DarkRoast(Beverage):
    """Concrete component: Dark Roast Coffee."""
    def get_description(self) -> str:
        return "Dark Roast Coffee"

    def get_cost(self) -> float:
        return 0.99


class Decaf(Beverage):
    """Concrete component: Decaf Coffee."""
    def get_description(self) -> str:
        return "Decaf Coffee"

    def get_cost(self) -> float:
        return 1.05


# Abstract Decorator
class CondimentDecorator(Beverage):
    """
    Abstract decorator for condiments/add-ons.
    配料/附加品的抽象装饰器。
    """
    def __init__(self, beverage: Beverage):
        self._beverage = beverage

    @abstractmethod
    def get_description(self) -> str:
        pass


# Concrete Decorators
class Milk(CondimentDecorator):
    """Concrete decorator: Milk."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Milk"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.10


class Mocha(CondimentDecorator):
    """Concrete decorator: Mocha (chocolate)."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Mocha"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.20


class Soy(CondimentDecorator):
    """Concrete decorator: Soy milk."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Soy"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.15


class Whip(CondimentDecorator):
    """Concrete decorator: Whipped cream."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Whip"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.10


class Caramel(CondimentDecorator):
    """Concrete decorator: Caramel syrup."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Caramel"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.25


class ExtraShot(CondimentDecorator):
    """Concrete decorator: Extra espresso shot."""
    def get_description(self) -> str:
        return f"{self._beverage.get_description()}, Extra Shot"

    def get_cost(self) -> float:
        return self._beverage.get_cost() + 0.50


def print_order(beverage: Beverage) -> None:
    """Print order details."""
    print(f"  {beverage.get_description()}")
    print(f"  Total: ${beverage.get_cost():.2f}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Decorator Pattern Demo ===\n")

    # Order 1: Simple espresso
    print("Order 1: Plain Espresso")
    beverage1 = Espresso()
    print_order(beverage1)
    print()

    # Order 2: Dark Roast with double mocha and whip
    print("Order 2: Dark Roast with double Mocha and Whip")
    beverage2 = DarkRoast()
    beverage2 = Mocha(beverage2)     # First mocha
    beverage2 = Mocha(beverage2)     # Second mocha
    beverage2 = Whip(beverage2)      # Whipped cream
    print_order(beverage2)
    print()

    # Order 3: House Blend with soy, mocha, and whip
    print("Order 3: House Blend with Soy, Mocha, and Whip")
    beverage3 = HouseBlend()
    beverage3 = Soy(beverage3)
    beverage3 = Mocha(beverage3)
    beverage3 = Whip(beverage3)
    print_order(beverage3)
    print()

    # Order 4: Fancy Espresso with everything
    print("Order 4: Fancy Espresso (Extra Shot, Milk, Mocha, Caramel, Whip)")
    beverage4 = Espresso()
    beverage4 = ExtraShot(beverage4)
    beverage4 = Milk(beverage4)
    beverage4 = Mocha(beverage4)
    beverage4 = Caramel(beverage4)
    beverage4 = Whip(beverage4)
    print_order(beverage4)
    print()

    # Order 5: Decaf with soy (health conscious)
    print("Order 5: Decaf with Soy")
    beverage5 = Decaf()
    beverage5 = Soy(beverage5)
    print_order(beverage5)
```

---

## 5. Facade Pattern (外观模式)

**Provide a unified interface to a set of interfaces in a subsystem, making the subsystem easier to use.**

### 中文详解

外观模式是一种结构型设计模式，它为子系统中的一组接口提供一个一致的界面，定义一个高层接口，使得这一子系统更加容易使用。

**适用场景：**
- 当需要为一个复杂子系统提供一个简单接口时
- 当客户程序与多个子系统之间存在很大的依赖性时
- 当需要构建一个层次结构的子系统时
- 例如：编译器、视频转换库、家庭影院系统

**优点：**
- 减少了系统的相互依赖
- 提高了灵活性：不管子系统如何变化，只要不影响外观对象
- 提高了安全性：外观只暴露必要的接口

**缺点：**
- 不能很好地限制客户使用子系统类
- 在不引入抽象外观类的情况下，增加新的子系统可能需要修改外观类

### Structure Diagram

```
+---------+                    +---------------+
| Client  |                    |    Facade     |
+---------+                    +---------------+
     |                         | + operation() |
     |                         +---------------+
     |       simple                   |
     +-------interface--------------->|
                                      |
              +-------+---------------+-------+
              |       |               |       |
              v       v               v       v
         +------+ +------+       +------+ +------+
         | Sub  | | Sub  |       | Sub  | | Sub  |
         |  A   | |  B   |       |  C   | |  D   |
         +------+ +------+       +------+ +------+
         
         <-------- Complex Subsystem -------->
```

**图解说明：**
- `Facade` 外观类，提供简化的接口
- `Subsystem A/B/C/D` 子系统类，实现复杂的功能
- Client 通过 Facade 访问子系统，无需直接与子系统交互
- Facade 协调子系统完成复杂操作

### Python Code Example

```python
"""
Facade Pattern Implementation in Python
外观模式的 Python 实现

Example: Home theater system
示例：家庭影院系统
"""


# Subsystem classes
class TV:
    """Subsystem: Television."""
    def on(self) -> str:
        return "TV is ON"

    def off(self) -> str:
        return "TV is OFF"

    def set_input(self, input_source: str) -> str:
        return f"TV input set to {input_source}"


class SoundSystem:
    """Subsystem: Sound System."""
    def on(self) -> str:
        return "Sound System is ON"

    def off(self) -> str:
        return "Sound System is OFF"

    def set_volume(self, level: int) -> str:
        return f"Sound System volume set to {level}"

    def set_surround_mode(self, mode: str) -> str:
        return f"Sound System surround mode: {mode}"


class StreamingPlayer:
    """Subsystem: Streaming Player."""
    def on(self) -> str:
        return "Streaming Player is ON"

    def off(self) -> str:
        return "Streaming Player is OFF"

    def play(self, movie: str) -> str:
        return f"Streaming Player: Playing '{movie}'"

    def pause(self) -> str:
        return "Streaming Player: Paused"

    def stop(self) -> str:
        return "Streaming Player: Stopped"


class Lights:
    """Subsystem: Room Lights."""
    def on(self) -> str:
        return "Lights are ON"

    def off(self) -> str:
        return "Lights are OFF"

    def dim(self, level: int) -> str:
        return f"Lights dimmed to {level}%"


class Projector:
    """Subsystem: Projector."""
    def on(self) -> str:
        return "Projector is ON"

    def off(self) -> str:
        return "Projector is OFF"

    def set_mode(self, mode: str) -> str:
        return f"Projector mode: {mode}"


class PopcornMachine:
    """Subsystem: Popcorn Machine."""
    def on(self) -> str:
        return "Popcorn Machine is ON"

    def off(self) -> str:
        return "Popcorn Machine is OFF"

    def pop(self) -> str:
        return "Popcorn Machine: Popping corn!"


# Facade
class HomeTheaterFacade:
    """
    Facade for the home theater system.
    家庭影院系统的外观类。
    """
    def __init__(self):
        self._tv = TV()
        self._sound = SoundSystem()
        self._player = StreamingPlayer()
        self._lights = Lights()
        self._projector = Projector()
        self._popcorn = PopcornMachine()

    def watch_movie(self, movie: str) -> str:
        """
        Simplified interface to watch a movie.
        观看电影的简化接口。
        """
        actions = ["=== Starting Movie Mode ==="]
        
        # Prepare the room
        actions.append(self._popcorn.on())
        actions.append(self._popcorn.pop())
        actions.append(self._lights.dim(10))
        
        # Set up display
        actions.append(self._projector.on())
        actions.append(self._projector.set_mode("widescreen"))
        
        # Set up audio
        actions.append(self._sound.on())
        actions.append(self._sound.set_surround_mode("Dolby Atmos"))
        actions.append(self._sound.set_volume(50))
        
        # Start the movie
        actions.append(self._player.on())
        actions.append(self._player.play(movie))
        
        actions.append("=== Enjoy your movie! ===")
        return "\n  ".join(actions)

    def end_movie(self) -> str:
        """
        Simplified interface to end movie mode.
        结束电影模式的简化接口。
        """
        actions = ["=== Ending Movie Mode ==="]
        
        actions.append(self._player.stop())
        actions.append(self._player.off())
        actions.append(self._projector.off())
        actions.append(self._sound.off())
        actions.append(self._popcorn.off())
        actions.append(self._lights.on())
        
        actions.append("=== Movie mode ended ===")
        return "\n  ".join(actions)

    def listen_to_music(self) -> str:
        """
        Simplified interface for music mode.
        音乐模式的简化接口。
        """
        actions = ["=== Starting Music Mode ==="]
        
        actions.append(self._lights.dim(50))
        actions.append(self._sound.on())
        actions.append(self._sound.set_surround_mode("Stereo"))
        actions.append(self._sound.set_volume(40))
        
        actions.append("=== Music mode ready ===")
        return "\n  ".join(actions)

    def watch_tv(self, channel: str = "HDMI1") -> str:
        """
        Simplified interface for TV mode.
        电视模式的简化接口。
        """
        actions = ["=== Starting TV Mode ==="]
        
        actions.append(self._lights.dim(70))
        actions.append(self._tv.on())
        actions.append(self._tv.set_input(channel))
        actions.append(self._sound.on())
        actions.append(self._sound.set_surround_mode("TV Mode"))
        actions.append(self._sound.set_volume(30))
        
        actions.append("=== TV mode ready ===")
        return "\n  ".join(actions)

    def all_off(self) -> str:
        """
        Turn off all devices.
        关闭所有设备。
        """
        actions = ["=== Shutting down all systems ==="]
        
        actions.append(self._tv.off())
        actions.append(self._sound.off())
        actions.append(self._player.off())
        actions.append(self._projector.off())
        actions.append(self._popcorn.off())
        actions.append(self._lights.on())
        
        actions.append("=== All systems off ===")
        return "\n  ".join(actions)


# Client code demonstration
if __name__ == "__main__":
    print("=== Facade Pattern Demo ===\n")

    # Create the facade
    home_theater = HomeTheaterFacade()

    # Simple operations instead of controlling 6+ devices
    print("1. Watch a Movie:")
    print(f"  {home_theater.watch_movie('The Matrix')}")
    print()

    print("2. End Movie:")
    print(f"  {home_theater.end_movie()}")
    print()

    print("3. Watch TV:")
    print(f"  {home_theater.watch_tv('Cable')}")
    print()

    print("4. Shut Down Everything:")
    print(f"  {home_theater.all_off()}")
```

---

## 6. Flyweight Pattern (享元模式)

**Use sharing to support large numbers of fine-grained objects efficiently.**

### 中文详解

享元模式是一种结构型设计模式，它通过共享技术来有效地支持大量细粒度对象的复用。

**核心概念：**
- 内部状态（Intrinsic State）：存储在享元对象内部，不会随环境改变而改变，可以共享
- 外部状态（Extrinsic State）：随环境改变而改变，不可以共享，由客户端保存

**适用场景：**
- 程序需要生成大量相似对象时
- 对象中包含可以在多个对象间共享的重复状态时
- 例如：文本编辑器中的字符、游戏中的粒子系统、地图应用中的图标

**优点：**
- 极大减少内存占用
- 在需要大量相似对象时可以节省大量资源

**缺点：**
- 可能需要牺牲执行速度来换取内存
- 代码会变得复杂（需要分离内部状态和外部状态）

### Structure Diagram

```
+-------------------+        +-------------------+
|  FlyweightFactory |        |    Flyweight      |
+-------------------+        |   <<interface>>   |
| - flyweights: Map |        +-------------------+
+-------------------+        | + operation(      |
| + get_flyweight() |------->|     extrinsic)    |
+-------------------+        +-------------------+
                                      ^
              +------------------------------------------+
              |                       |                  |
    +-------------------+   +-------------------+        |
    |ConcreteFlyweight  |   |ConcreteFlyweight  |        |
    |       A           |   |       B           |        |
    +-------------------+   +-------------------+        |
    | - intrinsic_state |   | - intrinsic_state |        |
    +-------------------+   +-------------------+        |
    | + operation(      |   | + operation(      |        |
    |     extrinsic)    |   |     extrinsic)    |        |
    +-------------------+   +-------------------+        |
                                                         |
                                            +-----------------------+
                                            | UnsharedFlyweight     |
                                            | (optional, not shared)|
                                            +-----------------------+
```

**图解说明：**
- `Flyweight` 享元接口，接收外部状态
- `ConcreteFlyweight` 具体享元，存储内部状态
- `FlyweightFactory` 享元工厂，管理享元对象池
- 内部状态存储在享元中（共享），外部状态由客户端传入

### Python Code Example

```python
"""
Flyweight Pattern Implementation in Python
享元模式的 Python 实现

Example: Text editor with shared character formatting
示例：具有共享字符格式的文本编辑器
"""

from typing import Dict, List, Tuple
import sys


class CharacterStyle:
    """
    Flyweight: Shared character style (intrinsic state).
    享元：共享的字符样式（内部状态）。
    """
    def __init__(self, font: str, size: int, color: str, bold: bool, italic: bool):
        # Intrinsic state - shared among many characters
        self._font = font
        self._size = size
        self._color = color
        self._bold = bold
        self._italic = italic

    def render(self, char: str, position: Tuple[int, int]) -> str:
        """
        Render a character with this style at the given position.
        position is extrinsic state - different for each character.
        """
        style_desc = f"{self._font} {self._size}pt {self._color}"
        if self._bold:
            style_desc += " bold"
        if self._italic:
            style_desc += " italic"
        return f"'{char}' at {position} with [{style_desc}]"

    def __str__(self) -> str:
        return f"Style({self._font}, {self._size}, {self._color})"

    def get_key(self) -> str:
        """Get a unique key for this style combination."""
        return f"{self._font}_{self._size}_{self._color}_{self._bold}_{self._italic}"


class StyleFactory:
    """
    Flyweight Factory: Manages character style objects.
    享元工厂：管理字符样式对象。
    """
    _styles: Dict[str, CharacterStyle] = {}

    @classmethod
    def get_style(cls, font: str, size: int, color: str,
                  bold: bool = False, italic: bool = False) -> CharacterStyle:
        """
        Get or create a character style.
        获取或创建字符样式。
        """
        key = f"{font}_{size}_{color}_{bold}_{italic}"

        if key not in cls._styles:
            cls._styles[key] = CharacterStyle(font, size, color, bold, italic)
            print(f"  [Factory] Created new style: {key}")
        else:
            print(f"  [Factory] Reusing existing style: {key}")

        return cls._styles[key]

    @classmethod
    def get_style_count(cls) -> int:
        """Get the number of unique styles created."""
        return len(cls._styles)

    @classmethod
    def clear(cls) -> None:
        """Clear all cached styles."""
        cls._styles.clear()


class Character:
    """
    Context: Represents a character in the document.
    上下文：表示文档中的一个字符。
    """
    def __init__(self, char: str, x: int, y: int, style: CharacterStyle):
        self._char = char              # Extrinsic (could share if same char)
        self._x = x                    # Extrinsic - unique position
        self._y = y                    # Extrinsic - unique position
        self._style = style            # Reference to shared flyweight

    def render(self) -> str:
        """Render this character."""
        return self._style.render(self._char, (self._x, self._y))


class TextDocument:
    """
    Client: Uses flyweights to render text efficiently.
    客户端：使用享元高效渲染文本。
    """
    def __init__(self):
        self._characters: List[Character] = []

    def add_character(self, char: str, x: int, y: int,
                      font: str = "Arial", size: int = 12,
                      color: str = "black", bold: bool = False,
                      italic: bool = False) -> None:
        """Add a character to the document."""
        style = StyleFactory.get_style(font, size, color, bold, italic)
        character = Character(char, x, y, style)
        self._characters.append(character)

    def add_text(self, text: str, start_x: int, start_y: int,
                 font: str = "Arial", size: int = 12,
                 color: str = "black", bold: bool = False,
                 italic: bool = False) -> None:
        """Add a string of text."""
        x = start_x
        for char in text:
            self.add_character(char, x, start_y, font, size, color, bold, italic)
            x += size  # Simple spacing based on font size

    def render(self) -> List[str]:
        """Render all characters."""
        return [char.render() for char in self._characters]

    def get_character_count(self) -> int:
        """Get total number of characters."""
        return len(self._characters)


def calculate_memory_savings():
    """
    Demonstrate memory savings with flyweight pattern.
    演示享元模式的内存节省。
    """
    # Estimate memory per style object (simplified)
    style_size = sys.getsizeof("Arial") + sys.getsizeof(12) + \
                 sys.getsizeof("black") + sys.getsizeof(True) * 2 + 100  # overhead

    return style_size


# Client code demonstration
if __name__ == "__main__":
    print("=== Flyweight Pattern Demo ===\n")

    # Clear any existing styles
    StyleFactory.clear()

    print("1. Creating a document with shared styles:")
    doc = TextDocument()

    # Add text with different styles
    print("\n  Adding 'Hello' in Arial 12pt black:")
    doc.add_text("Hello", 0, 0, "Arial", 12, "black")

    print("\n  Adding 'World' in Arial 12pt black (reuses style):")
    doc.add_text("World", 100, 0, "Arial", 12, "black")

    print("\n  Adding 'Bold' in Arial 12pt black bold:")
    doc.add_text("Bold", 0, 20, "Arial", 12, "black", bold=True)

    print("\n  Adding 'Title' in Times 24pt blue bold:")
    doc.add_text("Title", 0, 40, "Times", 24, "blue", bold=True)

    print("\n  Adding 'Note' in Arial 12pt black (reuses style):")
    doc.add_text("Note", 200, 0, "Arial", 12, "black")

    # Statistics
    print(f"\n2. Statistics:")
    print(f"   Total characters: {doc.get_character_count()}")
    print(f"   Unique styles created: {StyleFactory.get_style_count()}")

    # Memory analysis
    style_size = calculate_memory_savings()
    without_flyweight = doc.get_character_count() * style_size
    with_flyweight = StyleFactory.get_style_count() * style_size

    print(f"\n3. Memory Analysis (estimated):")
    print(f"   Without Flyweight: ~{without_flyweight} bytes "
          f"({doc.get_character_count()} chars × {style_size} bytes)")
    print(f"   With Flyweight: ~{with_flyweight} bytes "
          f"({StyleFactory.get_style_count()} styles × {style_size} bytes)")
    print(f"   Savings: ~{without_flyweight - with_flyweight} bytes "
          f"({100 * (1 - with_flyweight/without_flyweight):.1f}%)")

    # Render sample
    print("\n4. Sample rendering (first 5 characters):")
    for rendered in doc.render()[:5]:
        print(f"   {rendered}")
```

---

## 7. Proxy Pattern (代理模式)

**Provide a surrogate or placeholder for another object to control access to it.**

### 中文详解

代理模式是一种结构型设计模式，它为其他对象提供一种代理以控制对这个对象的访问。

**代理类型：**
- 远程代理（Remote Proxy）：为远程对象提供本地代表
- 虚拟代理（Virtual Proxy）：延迟创建开销大的对象
- 保护代理（Protection Proxy）：控制对原始对象的访问权限
- 智能引用代理（Smart Reference）：在访问对象时执行额外操作

**适用场景：**
- 远程代理：分布式系统中的远程服务调用
- 虚拟代理：图片懒加载、大文件延迟读取
- 保护代理：权限控制
- 例如：数据库连接池、缓存代理、日志代理

**优点：**
- 可以在客户端毫无察觉的情况下控制服务对象
- 可以在客户端不知情的情况下增加额外功能
- 开闭原则：可以在不修改服务或客户端的情况下创建新代理

**缺点：**
- 代码可能会变得复杂
- 服务响应可能会延迟

### Structure Diagram

```
+-------------------+            +-------------------+
|      Client       |            |      Subject      |
+-------------------+            |   <<interface>>   |
        |                        +-------------------+
        |                        | + request()       |
        |                        +-------------------+
        |                                 ^
        |                                 |
        |                    +------------+------------+
        |                    |                         |
        v                    |                         |
+-------------------+        |               +-------------------+
|      Proxy        |--------+               |   RealSubject     |
+-------------------+                        +-------------------+
| - realSubject     |----------------------->| + request()       |
+-------------------+   controls access      +-------------------+
| + request()       |
| + check_access()  |
| + log_access()    |
+-------------------+
```

**图解说明：**
- `Subject` 定义 RealSubject 和 Proxy 的公共接口
- `RealSubject` 定义 Proxy 所代表的真实对象
- `Proxy` 保存对 RealSubject 的引用，控制对其访问
- Client 通过 Subject 接口与 Proxy 交互

### Python Code Example

```python
"""
Proxy Pattern Implementation in Python
代理模式的 Python 实现

Examples: Virtual proxy, Protection proxy, Logging proxy
示例：虚拟代理、保护代理、日志代理
"""

from abc import ABC, abstractmethod
from datetime import datetime
from typing import Optional
import time


# Subject interface
class Image(ABC):
    """
    Subject interface for images.
    图像的主题接口。
    """
    @abstractmethod
    def display(self) -> str:
        pass

    @abstractmethod
    def get_filename(self) -> str:
        pass


# Real Subject
class RealImage(Image):
    """
    Real subject: High resolution image.
    真实主题：高分辨率图像。
    """
    def __init__(self, filename: str):
        self._filename = filename
        self._load_from_disk()

    def _load_from_disk(self) -> None:
        """Simulate expensive loading operation."""
        print(f"    [RealImage] Loading {self._filename} from disk...")
        time.sleep(0.5)  # Simulate I/O delay
        print(f"    [RealImage] {self._filename} loaded successfully")

    def display(self) -> str:
        return f"Displaying {self._filename}"

    def get_filename(self) -> str:
        return self._filename


# Virtual Proxy (Lazy Loading)
class ImageProxy(Image):
    """
    Virtual Proxy: Delays loading until needed.
    虚拟代理：延迟加载直到需要时。
    """
    def __init__(self, filename: str):
        self._filename = filename
        self._real_image: Optional[RealImage] = None

    def display(self) -> str:
        if self._real_image is None:
            print(f"  [Proxy] First access, creating RealImage...")
            self._real_image = RealImage(self._filename)
        return self._real_image.display()

    def get_filename(self) -> str:
        return self._filename


# Protection Proxy
class Document(ABC):
    """Subject interface for documents."""
    @abstractmethod
    def read(self) -> str:
        pass

    @abstractmethod
    def write(self, content: str) -> str:
        pass


class SensitiveDocument(Document):
    """Real subject: Sensitive document."""
    def __init__(self, name: str, content: str):
        self._name = name
        self._content = content

    def read(self) -> str:
        return f"Content of {self._name}: {self._content}"

    def write(self, content: str) -> str:
        self._content = content
        return f"Updated {self._name}"


class ProtectedDocumentProxy(Document):
    """
    Protection Proxy: Controls access based on user role.
    保护代理：根据用户角色控制访问。
    """
    def __init__(self, document: SensitiveDocument, user_role: str):
        self._document = document
        self._user_role = user_role
        self._access_log = []

    def _log_access(self, operation: str, allowed: bool) -> None:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        status = "ALLOWED" if allowed else "DENIED"
        self._access_log.append(f"[{timestamp}] {operation}: {status}")

    def read(self) -> str:
        # All roles can read
        self._log_access(f"READ by {self._user_role}", True)
        return self._document.read()

    def write(self, content: str) -> str:
        # Only admin can write
        if self._user_role == "admin":
            self._log_access(f"WRITE by {self._user_role}", True)
            return self._document.write(content)
        else:
            self._log_access(f"WRITE by {self._user_role}", False)
            return f"Access denied: {self._user_role} cannot write"

    def get_access_log(self) -> list:
        return self._access_log.copy()


# Logging/Caching Proxy
class DataService(ABC):
    """Subject interface for data service."""
    @abstractmethod
    def fetch_data(self, query: str) -> str:
        pass


class DatabaseService(DataService):
    """Real subject: Database service."""
    def fetch_data(self, query: str) -> str:
        time.sleep(0.3)  # Simulate database query
        return f"Data for query: {query}"


class CachingProxy(DataService):
    """
    Caching Proxy: Caches results and logs access.
    缓存代理：缓存结果并记录访问。
    """
    def __init__(self, service: DataService):
        self._service = service
        self._cache = {}
        self._stats = {"hits": 0, "misses": 0}

    def fetch_data(self, query: str) -> str:
        timestamp = datetime.now().strftime("%H:%M:%S")

        if query in self._cache:
            self._stats["hits"] += 1
            print(f"  [{timestamp}] Cache HIT for '{query}'")
            return self._cache[query]

        self._stats["misses"] += 1
        print(f"  [{timestamp}] Cache MISS for '{query}', fetching...")
        result = self._service.fetch_data(query)
        self._cache[query] = result
        return result

    def get_stats(self) -> dict:
        return self._stats.copy()

    def clear_cache(self) -> None:
        self._cache.clear()


# Client code demonstration
if __name__ == "__main__":
    print("=== Proxy Pattern Demo ===\n")

    # Example 1: Virtual Proxy (Lazy Loading)
    print("1. Virtual Proxy (Lazy Loading):")
    print("  Creating image proxies (images not loaded yet)...")
    images = [
        ImageProxy("photo1.jpg"),
        ImageProxy("photo2.jpg"),
        ImageProxy("photo3.jpg"),
    ]
    print(f"  Created {len(images)} proxies without loading")
    print()

    print("  Displaying first image (triggers loading):")
    print(f"  {images[0].display()}")
    print()

    print("  Displaying first image again (already loaded):")
    print(f"  {images[0].display()}")
    print()

    # Example 2: Protection Proxy
    print("2. Protection Proxy:")
    secret_doc = SensitiveDocument("secret.txt", "Top secret content")

    print("  User role: viewer")
    viewer_proxy = ProtectedDocumentProxy(secret_doc, "viewer")
    print(f"  Read: {viewer_proxy.read()}")
    print(f"  Write: {viewer_proxy.write('New content')}")
    print()

    print("  User role: admin")
    admin_proxy = ProtectedDocumentProxy(secret_doc, "admin")
    print(f"  Read: {admin_proxy.read()}")
    print(f"  Write: {admin_proxy.write('Updated by admin')}")
    print()

    print("  Access logs (viewer):", viewer_proxy.get_access_log())
    print()

    # Example 3: Caching Proxy
    print("3. Caching Proxy:")
    db_service = DatabaseService()
    cached_service = CachingProxy(db_service)

    queries = ["SELECT * FROM users", "SELECT * FROM products",
               "SELECT * FROM users", "SELECT * FROM orders",
               "SELECT * FROM users"]

    for query in queries:
        result = cached_service.fetch_data(query)
        print(f"  Result: {result}")

    print(f"\n  Cache Statistics: {cached_service.get_stats()}")
```

---

## Summary Table

| Pattern | Purpose | Key Benefit |
|---------|---------|-------------|
| **Adapter** | Interface conversion | Integrates incompatible classes |
| **Bridge** | Separate abstraction from implementation | Independent variation |
| **Composite** | Tree structures | Uniform treatment of parts/wholes |
| **Decorator** | Dynamic behavior addition | Flexible alternative to subclassing |
| **Facade** | Simplified interface | Reduces complexity |
| **Flyweight** | Share fine-grained objects | Memory optimization |
| **Proxy** | Control access | Lazy loading, protection, logging |

---

*Next: See `design_patterns_behavioral.md` for behavioral patterns.*

