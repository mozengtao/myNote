[Game Programming Patterns](https://gameprogrammingpatterns.com/contents.html)  
[Java Design Patterns](https://java-design-patterns.com/patterns/)  
[14 Patterns to Ace Any Coding Interview Question](https://hackernoon.com/14-patterns-to-ace-any-coding-interview-question-c5bb3357f6ed)  
[Design Patterns](https://sourcemaking.com/design_patterns)  
[**Design Pattern Online Training using .NET**](https://dotnettutorials.net/lesson/design-patterns-online-training/)  
[Books](https://github.com/gmoral/Books)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  

## SOLID原则
1. SRP(Single Responsibility Principle单一职责原则)
```python
# SRP
class Journal:
    def __init__(self):
        self.entries = []
        self.count = 0

    def add_entry(self, text):
        self.count += 1
        self.entries.append(f"{self.count}: {text}")

    def remove_entry(self, index):
        del self.entries[index]

    def __str__(self):
        return "\n".join(self.entries)

class PersistenceManager:
    @staticmethod
    def save_to_file(journal, filename):
        with open(filename, "w") as f:
            f.write(str(journal))

j = Journal()
j.add_entry("I cried today.")
j.add_entry("I ate a bug.")
print(f"Journal entries:\n{j}")

file = r'journal.txt'
PersistenceManager.save_to_file(j, file)

with open(file, "r") as f:
    print(f.read())
```

- OCP(Open-Closed Principle:开闭原则) (Open for extension, closed for modification)
```python
from enum import Enum

class Color(Enum):
    RED = 1
    GREEN = 2
    BLUE = 3

class Size(Enum):
    SMALL = 1
    MEDIUM = 2
    LARGE = 3

class Product:
    def __init__(self, name, color, size):
        self.name = name
        self.color = color
        self.size = size

class ProductFilter:
    def filter(self, products, spec):
        for product in products:
            if spec.is_satisfied(product):
                yield product

class Specification:
    def is_satisfied(self, item):
        pass

    def __and__(self, other):
        return AndSpecification(self, other)

class Filter:
    def filter(self, items, spec):
        pass

class ColorSpecification(Specification):
    def __init__(self, color):
        self.color = color

    def is_satisfied(self, item):
        return item.color == self.color

class SizeSpecification(Specification):
    def __init__(self, size):
        self.size = size

    def is_satisfied(self, item):
        return item.size == self.size

class AndSpecification(Specification):
    def __init__(self, *args):
        self.args = args

    def is_satisfied(self, item):
        return all(spec.is_satisfied(item) for spec in self.args)

class BetterFilter(Filter):
    def filter(self, items, spec):
        for item in items:
            if spec.is_satisfied(item):
                yield item

if __name__ == "__main__":
    apple = Product("Apple", Color.RED, Size.SMALL)
    tree = Product("Tree", Color.GREEN, Size.LARGE)
    house = Product("House", Color.BLUE, Size.MEDIUM)

    products = [apple, tree, house]

    pf = BetterFilter()
    print("Green products:")
    green_spec = ColorSpecification(Color.GREEN)
    for p in pf.filter(products, green_spec):
        print(f" - {p.name} is green")

    print("Large products:")
    large_spec = SizeSpecification(Size.LARGE)
    for p in pf.filter(products, large_spec):
        print(f" - {p.name} is large")

    print("Large green products:")
    large_green_spec = AndSpecification(SizeSpecification(Size.LARGE), ColorSpecification(Color.GREEN))
    for p in pf.filter(products, large_green_spec):
        print(f" - {p.name} is large and green")

    print("Large green products:")
    large_green_spec = SizeSpecification(Size.LARGE) and ColorSpecification(Color.GREEN)
    for p in pf.filter(products, large_green_spec):
        print(f" - {p.name} is large and green")
```

- LSP(里氏替换原则) (Liskov Substitution Principle)
```python

class Rectangle:
    def __init__(self, width, height):
        self._width = width
        self._height = height

    @property
    def area(self):
        return self._width * self._height

    def __str__(self):
        return f"Rectangle(width={self._width}, height={self._height})"

    @property
    def width(self):
        return self._width

    @width.setter
    def width(self, value):
        self._width = value

    @property
    def height(self):
        return self._height

    @height.setter
    def height(self, value):
        self._height = value

class Square(Rectangle):
    def __init__(self, size):
        Rectangle.__init__(self, size, size)

    def __str__(self):
        return f"Square(size={self._width})"

    @Rectangle.width.setter
    def width(self, value):
        self._width = self._height = value

    @Rectangle.height.setter
    def height(self, value):
        self._width = self._height = value

def use_it(rc):
    w = rc.width
    rc.height = 10
    expected = int(w * 10)
    print(f"Expected an area of {expected}, got {rc.area}")

rc = Rectangle(2, 3)
use_it(rc)

sq = Square(5)
use_it(sq)
```

- ISP(Interface Segregation Principle:接口隔离原则) (Interface Segregation Principle)

```python
from abc import abstractmethod

class Printer:
    @abstractmethod
    def print(self, document):
        pass

class Scanner:
    @abstractmethod
    def scan(self, document):
        pass

class PrinterScanner(Printer, Scanner):
    @abstractmethod
    def print(self, document):
        pass

    @abstractmethod
    def scan(self, document):
        pass

class PrinterScannerMachine(PrinterScanner):
    def __init__(self, printer, scanner):
        self.printer = printer
        self.scanner = scanner

    def print(self, document):
        self.printer.print(document)

    def scan(self, document):
        self.scanner.scan(document)

class PrinterImpl(Printer):
    def print(self, document):
        print(f"Printing {document}")

class ScannerImpl(Scanner):
    def scan(self, document):
        print(f"Scanning {document}")

myPrinter = PrinterImpl()
myScanner = ScannerImpl()
myPrinterScanner = PrinterScannerMachine(myPrinter, myScanner)
myPrinterScanner.print("Hello, world!")
myPrinterScanner.scan("Hello, world!")
```

- DIP(Dependency Inversion Principle:依赖倒置原则)
	- 高层模块不应该依赖于低层模块，两者都应该依赖于抽象

```python
from enum import Enum
from abc import abstractmethod

class Relationship(Enum):
    PARENT = 0
    CHILD = 1
    SIBLING = 2

class Person:
    def __init__(self, name):
        self.name = name

# abstraction interface
class RelationshipBrowser:
    @abstractmethod
    def find_all_children_of(self, name):
        pass

# low-level module
class Relationships(RelationshipBrowser):
    def __init__(self):
        # low-level implementation
        self.relations = []

    def add_parent_and_child(self, parent, child):
        self.relations.append((parent, Relationship.PARENT, child))

    def find_all_children_of(self, name):
        for relation in self.relations:
            if relation[0].name == name and relation[1] == Relationship.PARENT:
                yield relation[2].name

# high-level module
class Research:
    def __init__(self, browser):
        for p in browser.find_all_children_of("John"):
            print(f"John has a child called {p}")

if __name__ == "__main__":
    parent = Person("John")
    child1 = Person("Chris")
    child2 = Person("Matt")
    relationships = Relationships()
    relationships.add_parent_and_child(parent, child1)
    relationships.add_parent_and_child(parent, child2)
    research = Research(relationships)
```

## Patterns

## Builder Pattern
- When piecewise object construction is complicated, provide an API for doing it succinctly.
```python
# original code
words = ['hello', 'world']
parts = ['<ul>']
for w in words:
    parts.append(f'  <li>{w}</li>')
parts.append('</ul>')
html = '\n'.join(parts)
print(html)

# Improve with Builder Pattern
class HtmlElement:
    indent_size = 2

    def __init__(self, name, text):
        self.name = name
        self.text = text
        self.elements = []

    def __str(self, indent):
        lines = []
        i = ' ' * indent * self.indent_size
        lines.append(f'{i}<{self.name}>')

        if self.text:
            t = ' ' * (indent + 1) * self.indent_size
            lines.append(f'{t}{self.text}')

        for e in self.elements:
            lines.append(e.__str(indent + 1))

        lines.append(f'{i}</{self.name}>')
        return '\n'.join(lines)

    def __str__(self):
        return self.__str(0)

class HtmlBuilder:
    def __init__(self, root_name):
        self.root = HtmlElement(root_name, '')

    def add_child(self, child_name, child_text):
        self.root.elements.append(HtmlElement(child_name, child_text))

    def add_child_fluent(self, child_name, child_text):
        self.root.elements.append(HtmlElement(child_name, child_text))
        return self

    def __str__(self):
        return str(self.root)

    @staticmethod
    def create(name):
        return HtmlBuilder(name)

#builder = HtmlBuilder('ul')
builder = HtmlBuilder.create('ul')
#builder.add_child('li', 'hello')
#builder.add_child('li', 'world')
builder.add_child_fluent('li', 'hello')\
    .add_child_fluent('li', 'world')
print(builder)
```

## DDD
![领域驱动设计](./design_patterns/Domain_Driven_Design_in_C.md)  
![]()  
![]()  
1. 什么是 DDD
    DDD 核心理念：让代码"说业务语言"，业务驱动而非技术驱动
2. DDD 核心概念
    实体、值对象、聚合、聚合根、仓储、领域服务、领域事件
3. 传统方式 vs DDD 对比
    以数据为中心 vs 以领域为中心的代码对比

- DDD 核心概念
```
┌─────────────────────────────────────────────────────────┐
│                    Aggregate (聚合)                     │
│                                                         │
│   ┌───────────────────────────────────────────────┐     │
│   │        Aggregate Root (聚合根)                │     │
│   │        Order (订单)                           │     │
│   │                                               │     │
│   │   + confirm()   (业务方法)                    │     │
│   │   + cancel()                                  │     │
│   │   + add_item()                                │     │
│   └───────────────────────────────────────────────┘     │
│                         │                               │
│                         ▼                               │
│   ┌───────────────────────────────────────────────┐     │
│   │        Entity (实体) - OrderItem              │     │
│   │        Value Object (值对象) - Money          │     │
│   └───────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

- 示例代码结构
```
order_system/
├── domain/              # 领域层 - 核心业务逻辑
│   ├── order.h/c        # 订单聚合根
│   ├── order_item.h/c   # 订单项实体
│   ├── money.h/c        # 金额值对象
│   └── order_events.h   # 领域事件
├── repository/          # 仓储层 - 持久化
│   └── order_repository.h/c
├── service/             # 应用服务层
│   └── order_service.h/c
└── main.c               # 演示程序
```

- DDD 的 5 大好处
好处	        说明
业务逻辑集中	 所有规则在领域对象中，修改只需改一处
代码即文档	     order_confirm() 比 UPDATE status=1 更易懂
易于测试	    不依赖数据库，毫秒级单元测试
技术解耦	    换数据库不改业务代码
统一语言	    业务人员和开发人员使用相同术语