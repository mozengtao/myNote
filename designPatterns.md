[Design Patterns for Dummies](https://github.com/hraverkar/books)  
[Dive-Into-Design-Patterns---Code-Samples](https://github.com/arvi9/Dive-Into-Design-Patterns---Code-Samples/tree/master)  
[Design Patterns Explained simply](https://github.com/hraverkar/books/tree/master)  
[]()  
[]()  
[]()  



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
[Dive into Systems](https://diveintosystems.org/)  
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
- 核心思想
    C语言实现设计模式的本质是「用结构体模拟对象、函数指针实现多态、封装函数控制访问」，最终目标是解耦、复用、可扩展

- 创建型模式 (Creational Patterns): 聚焦对象创建逻辑
![构建器模式](builder_pattern.md)  
![工厂方法模式](factory_method_pattern.md)  
![抽象工厂模式](abstract_factory_pattern.md)  
![原型模式](prototype_pattern.md)  
![单例模式](singleton_pattern.md)  

- 结构型模式 (Structural Patterns):聚焦对象/模块的组合关系
![适配器模式](adapter_pattern.md)  
![桥接模式](bridge_pattern.md)  
![装饰器模式](decorator_pattern.md)  
![外观模式](facade_pattern.md)  
![代理模式](proxy_pattern.md)  

- 行为型模式 (Behavioral Patterns):聚焦对象间的交互与行为
![命令模式](command_pattern.md)  
![迭代器模式](iterator_pattern.md)  
![备忘录模式](memento_pattern.md)  
![观察者模式](observer_pattern.md)  
![策略模式](strategy_pattern.md)  
![访问者模式](visitor_pattern.md)  
![](./c_tips/patterns/)  
![]()  
![]()  
![]()  
![]()  

- 创建型模式 (Creational Patterns)
![单例模式](./c_tips/patterns/01_singleton.md)  
![工厂模式](./c_tips/patterns/02_factory.md)  
![原型模式](./c_tips/patterns/03_prototype.md)  

- 结构型模式 (Structural Patterns)
![适配器模式](./c_tips/patterns/04_adapter.md)  
![装饰器模式](./c_tips/patterns/05_decorator.md)  
![组合模式](./c_tips/patterns/06_composite.md)  
![桥接模式](./c_tips/patterns/07_bridge.md)  

- 行为型模式 (Behavioral Patterns)
![策略模式](./c_tips/patterns/08_strategy.md)  
![观察者模式](./c_tips/patterns/09_observer.md)  
![命令模式](./c_tips/patterns/10_command.md)  
![迭代器模式](./c_tips/patterns/11_iterator.md)  
![状态模式](./c_tips/patterns/12_state.md)  

- Kernel patterns
![README.md](./c_tips/kernel_patterns/README.md)  
![单例模式](./c_tips/kernel_patterns/01_singleton_pattern.md)  
![工厂模式](./c_tips/kernel_patterns/02_factory_pattern.md)  
![原型模式](./c_tips/kernel_patterns/03_prototype_pattern.md)  
![适配器模式](./c_tips/kernel_patterns/04_adapter_pattern.md)  
![装饰器模式](./c_tips/kernel_patterns/05_decorator_pattern.md)  
![组合模式](./c_tips/kernel_patterns/06_composite_pattern.md)  
![桥接模式](./c_tips/kernel_patterns/07_bridge_pattern.md)  
![策略模式](./c_tips/kernel_patterns/08_strategy_pattern.md)  
![观察者模式](./c_tips/kernel_patterns/09_observer_pattern.md)  
![命令模式](./c_tips/kernel_patterns/10_command_pattern.md)  
![迭代器模式](./c_tips/kernel_patterns/11_iterator_pattern.md)  
![状态模式](./c_tips/kernel_patterns/12_state_pattern.md)  

- Design Patterns for Embedded Systems in C (pdf)
![硬件访问模式](./design_patterns/embedded/01_hardware_access_patterns.md)  
![并发和资源管理模式](./design_patterns/embedded/02_concurrency_patterns.md)  
![状态机设计模式](./design_patterns/embedded/03_state_machine_patterns.md)  
![安全和可靠性模式](./design_patterns/embedded/04_safety_reliability_patterns.md)  


![Linux Operations Structure Pattern](./linux/linux_kernel_ops_pattern.md)  
![Linux Kernel 经典设计模式](./linux/linux_kernel_classic_patterns.md)  
![Linux Kernel 经典设计模式 对应 用户空间代码示例](./linux/kernel_patterns_userspace_examples.md)  
![]()  
![]()  
![]()  

## Builder Pattern
- When piecewise object construction is complicated, provide an API for doing it succinctly.
    - A buildder is a separate component for building an object step by step
    - Can either give builder an initializer or return it via a static function
    - To make builder fluent, return self
    - Different facets of an object can be built with different builders working in tandem via composition
```python
# original code
words = ['hello', 'world']
parts = ['<ul>']
for w in words:
    parts.append(f'  <li>{w}</li>')
parts.append('</ul>')
html = '\n'.join(parts)
print(html)

# =============================================================================
# 构建器模式 (Builder Pattern) - HTML 元素构建示例
# =============================================================================
# 核心思想：
#   1. 将复杂对象的构建过程与其表示分离
#   2. 通过构建器逐步添加内容，而不是一次性传入所有参数
#   3. 流畅接口 (Fluent Interface)：方法返回 self，支持链式调用
#   4. 静态工厂方法：提供更语义化的对象创建方式
# =============================================================================

class HtmlElement:
    """
    HTML 元素类：表示一个 HTML 标签节点
    支持嵌套子元素，形成树形结构
    """
    indent_size = 2  # 类变量：缩进空格数

    def __init__(self, name, text):
        self.name = name          # 标签名，如 'ul', 'li'
        self.text = text          # 标签内的文本内容
        self.elements = []        # 子元素列表（支持嵌套）

    def __str(self, indent):
        """
        私有递归方法：生成带缩进的 HTML 字符串
        indent: 当前缩进层级
        """
        lines = []
        i = ' ' * indent * self.indent_size
        lines.append(f'{i}<{self.name}>')           # 开始标签

        if self.text:
            t = ' ' * (indent + 1) * self.indent_size
            lines.append(f'{t}{self.text}')         # 文本内容（缩进一级）

        for e in self.elements:
            lines.append(e.__str(indent + 1))       # 递归渲染子元素

        lines.append(f'{i}</{self.name}>')          # 结束标签
        return '\n'.join(lines)

    def __str__(self):
        """公开的 __str__ 方法，从缩进 0 开始渲染"""
        return self.__str(0)


class HtmlBuilder:
    """
    HTML 构建器：负责逐步构建 HTML 元素树
    将构建逻辑封装，使用者无需关心 HtmlElement 的内部细节
    """
    def __init__(self, root_name):
        self.root = HtmlElement(root_name, '')  # 创建根元素

    def add_child(self, child_name, child_text):
        """普通方法：添加子元素，无返回值"""
        self.root.elements.append(HtmlElement(child_name, child_text))

    def add_child_fluent(self, child_name, child_text):
        """
        流畅接口方法：添加子元素并返回 self
        关键点：return self 使得可以链式调用多个方法
        """
        self.root.elements.append(HtmlElement(child_name, child_text))
        return self  # ← 这是流畅接口的关键！

    def __str__(self):
        return str(self.root)

    @staticmethod
    def create(name):
        """
        静态工厂方法：提供更语义化的创建方式
        HtmlBuilder.create('ul') 比 HtmlBuilder('ul') 更具可读性
        """
        return HtmlBuilder(name)


# =============================================================================
# 使用示例对比
# =============================================================================

# 方式1：直接构造 + 普通方法（需要多条语句）
# builder = HtmlBuilder('ul')
# builder.add_child('li', 'hello')
# builder.add_child('li', 'world')

# 方式2：静态工厂 + 流畅接口（一条链式语句）
builder = HtmlBuilder.create('ul')
builder.add_child_fluent('li', 'hello')\
    .add_child_fluent('li', 'world')

print(builder)
# 输出：
# <ul>
#   <li>
#     hello
#   </li>
#   <li>
#     world
#   </li>
# </ul>

# =============================================================================
# 分面构建器模式 (Faceted Builder Pattern)
# =============================================================================
# 核心思想：
#   1. 当一个对象有多个不同的"方面"(facet)需要构建时，可以为每个方面创建专门的子构建器
#   2. 所有子构建器共享同一个被构建对象的引用，从而协同工作
#   3. 通过属性(property)在不同的子构建器之间切换，实现流畅的链式调用

# 调用流程图:
# PersonBuilder
#     │
#     ├─.lives──► PersonAddressBuilder ──.at()/.with_postcode()/.in_city()
#     │                   │
#     │                   └─.works──► PersonJobBuilder
#     │
#     └─.works──► PersonJobBuilder ──.at()/.as_a()/.earning()
#                         │
#                         └─.lives──► PersonAddressBuilder
# =============================================================================

class Person:
    """被构建的目标对象，包含两个方面(facet)：地址信息和工作信息"""
    def __init__(self):
        # 地址方面 (address facet)
        self.street_address = None
        self.postcode = None
        self.city = None
        # 工作方面 (employment facet)
        self.company_name = None
        self.position = None
        self.annual_income = None

    def __str__(self):
        return f"Address: {self.street_address}, {self.postcode}, {self.city}\nCompany: {self.company_name}, {self.position}, {self.annual_income}"


class PersonBuilder:
    """
    主构建器：作为所有子构建器的基类
    关键点：通过 works 和 lives 属性提供对子构建器的访问入口
    """
    def __init__(self, person=Person()):
        self.person = person  # 所有构建器共享同一个 Person 实例

    @property
    def works(self):
        """切换到工作信息构建器，传递同一个 person 实例"""
        return PersonJobBuilder(self.person)

    @property
    def lives(self):
        """切换到地址信息构建器，传递同一个 person 实例"""
        return PersonAddressBuilder(self.person)

    def build(self):
        """完成构建，返回最终的 Person 对象"""
        return self.person


class PersonAddressBuilder(PersonBuilder):
    """
    地址子构建器：专门负责构建 Person 的地址方面
    继承自 PersonBuilder，因此可以通过 .works 切换到工作构建器
    """
    def __init__(self, person=Person()):
        super().__init__(person)

    def at(self, street_address):
        self.person.street_address = street_address
        return self  # 返回 self 实现链式调用
    
    def with_postcode(self, postcode):
        self.person.postcode = postcode
        return self

    def in_city(self, city):
        self.person.city = city
        return self


class PersonJobBuilder(PersonBuilder):
    """
    工作子构建器：专门负责构建 Person 的工作方面
    继承自 PersonBuilder，因此可以通过 .lives 切换到地址构建器
    """
    def __init__(self, person=Person()):
        super().__init__(person)

    def at(self, company_name):
        self.person.company_name = company_name
        return self

    def as_a(self, position):
        self.person.position = position
        return self
    
    def earning(self, annual_income):
        self.person.annual_income = annual_income
        return self


def main():
    # 使用示例：通过 .lives 和 .works 在不同构建器之间无缝切换
    pb = PersonBuilder()
    person = pb\
        .lives\
            .at("123 Main St")\
            .with_postcode("12345")\
            .in_city("Anytown")\
        .works\
            .at("Google")\
            .as_a("Engineer")\
            .earning(123456)\
        .build()
    print(person)

if __name__ == "__main__":
    main()

# =============================================================================
# Builder 建造者模式 - 通过继承实现多层建造者
# =============================================================================
# 核心思想：
# 1. 将复杂对象的构建过程与其表示分离
# 2. 通过继承链，每层 Builder 负责对象的一个方面（facet）
# 3. 使用流式接口（Fluent Interface）让调用更优雅
# =============================================================================

class Person:
    """被构建的目标对象 - 包含多个需要设置的属性"""
    def __init__(self):
        self.name = None
        self.job = None
        self.date_of_birth = None

    def __str__(self):
        return f"{self.name} born on {self.date_of_birth} works as a {self.job}"

    @staticmethod
    def new():
        """工厂方法：提供统一的入口来获取建造者
        返回最底层的建造者，确保所有构建方法都可用"""
        return PersonBirthDateBuilder()


# -----------------------------------------------------------------------------
# 建造者继承链：每层负责对象的一个方面
# PersonBuilder -> PersonInfoBuilder -> PersonJobBuilder -> PersonBirthDateBuilder
# -----------------------------------------------------------------------------

class PersonBuilder:
    """基础建造者：负责创建和持有 Person 实例"""
    def __init__(self):
        self.person = Person()  # 创建待构建的对象

    def build(self):
        """终结方法：返回构建完成的对象"""
        return self.person


class PersonInfoBuilder(PersonBuilder):
    """第一层：负责个人基本信息"""
    def called(self, name):
        self.person.name = name
        return self  # 返回 self 实现链式调用


class PersonJobBuilder(PersonInfoBuilder):
    """第二层：负责工作相关信息（继承了上层的 called 方法）"""
    def works_as_a(self, job):
        self.person.job = job
        return self  # 返回 self 实现链式调用


class PersonBirthDateBuilder(PersonJobBuilder):
    """第三层：负责出生日期（继承了上层所有方法）"""
    def born(self, date_of_birth):
        self.person.date_of_birth = date_of_birth
        return self  # 返回 self 实现链式调用


def main():
    # 使用示例：通过链式调用优雅地构建对象
    # Person.new() -> 获取建造者
    # .called().works_as_a().born() -> 链式设置各属性
    # .build() -> 获取最终对象
    person = Person.new().called("John").works_as_a("Engineer").born("1990-01-01").build()
    print(person)

if __name__ == "__main__":
    main()
```

## Factory
- A componet responsibility solely for the wholesale (not piecewise) creation of objects.
    - A factory method is a static method that creates objects.
    - A factory is any entity that can take care of object creation.
    - A factory can be external or reside inside an object as an inner class.
    - Hierarchies of factories can be used to create related objects.
```python
# =============================================================================
# 工厂方法模式 (Factory Method Pattern)
# =============================================================================
# 核心思想：
#   1. 用多个语义明确的静态方法替代单一复杂的构造函数
#   2. 方法名即文档：new_cartesian_point / new_polar_point 一目了然
#   3. 参数名更准确：(x, y) vs (r, theta)，避免歧义
#   4. 遵循单一职责原则：每个工厂方法只处理一种创建逻辑
# =============================================================================

from enum import Enum
from math import *


class CoordinateSystem(Enum):
    """坐标系类型枚举（用于演示反面模式）"""
    CARTESIAN = 1  # 笛卡尔坐标系 (x, y)
    POLAR = 2      # 极坐标系 (r, θ)


class Point:
    """二维平面上的点"""

    # -------------------------------------------------------------------------
    # ❌ 反面模式：在构造函数中用枚举/标志判断逻辑
    # -------------------------------------------------------------------------
    # 问题：
    #   1. 参数语义混乱：x 有时是 x 坐标，有时是半径 r
    #   2. 需要查文档才能知道如何使用
    #   3. 违反开闭原则：新增坐标系需要修改构造函数
    # -------------------------------------------------------------------------
    # def __init__(self, x, y, system=CoordinateSystem.CARTESIAN):
    #     if system == CoordinateSystem.CARTESIAN:
    #         self.x = x
    #         self.y = y
    #     elif system == CoordinateSystem.POLAR:
    #         self.x = x * cos(y)  # x 其实是 r，y 其实是 theta
    #         self.y = x * sin(y)

    def __init__(self, x, y):
        """私有化或简化构造函数，只接受最终的笛卡尔坐标"""
        self.x = x
        self.y = y

    def __str__(self):
        return f"Point(x={self.x}, y={self.y})"

    # -------------------------------------------------------------------------
    # ✅ 工厂方法模式：用语义明确的静态方法创建对象
    # -------------------------------------------------------------------------

    @staticmethod
    def new_cartesian_point(x, y):
        """
        工厂方法：从笛卡尔坐标创建点
        参数语义明确：x 就是 x 坐标，y 就是 y 坐标
        """
        return Point(x, y)

    @staticmethod
    def new_polar_point(r, theta):
        """
        工厂方法：从极坐标创建点
        参数语义明确：r 是半径，theta 是角度（弧度）
        内部自动转换为笛卡尔坐标
        """
        return Point(r * cos(theta), r * sin(theta))


if __name__ == "__main__":
    # 使用工厂方法创建点 - 代码自解释，无需注释
    p1 = Point.new_cartesian_point(1, 2)      # 笛卡尔坐标 (1, 2)
    p2 = Point.new_polar_point(1, 2)          # 极坐标 (r=1, θ=2弧度)
    print(p1)  # Point(x=1, y=2)
    print(p2)  # Point(x=-0.416..., y=0.909...)

# =============================================================================
# 内部工厂类模式 (Inner Factory Class Pattern)
# =============================================================================
# 核心思想：
#   1. 将工厂方法封装到内部类中，保持代码组织清晰
#   2. 通过类变量暴露单例工厂实例，简化调用
#   3. 调用方式：Point.factory.new_xxx() 比静态方法更具命名空间感
#   4. 便于扩展：工厂类可以持有状态、配置或缓存
# =============================================================================

from enum import Enum
from math import *


class CoordinateSystem(Enum):
    CARTESIAN = 1
    POLAR = 2


class Point:
    """二维平面上的点"""

    def __init__(self, x, y):
        self.x = x
        self.y = y

    def __str__(self):
        return f"Point(x={self.x}, y={self.y})"

    # -------------------------------------------------------------------------
    # 内部工厂类：将所有创建逻辑集中管理
    # -------------------------------------------------------------------------
    # 优势：
    #   1. 逻辑分组：所有工厂方法在一个类中，结构清晰
    #   2. 可扩展性：工厂类可以添加状态（如缓存已创建的点）
    #   3. 命名空间：Point.factory.xxx 明确表示这是创建操作
    # -------------------------------------------------------------------------
    class PointFactory:
        """Point 的内部工厂类"""

        def new_cartesian_point(self, x, y):
            """从笛卡尔坐标 (x, y) 创建点"""
            return Point(x, y)

        def new_polar_point(self, r, theta):
            """从极坐标 (r, θ) 创建点，自动转换为笛卡尔坐标"""
            return Point(r * cos(theta), r * sin(theta))

    # -------------------------------------------------------------------------
    # 类变量：暴露一个共享的工厂实例
    # -------------------------------------------------------------------------
    # 这样用户可以直接使用 Point.factory，无需每次创建新的工厂对象
    # -------------------------------------------------------------------------
    factory = PointFactory()


# class PointFactory:
#     """Point 的外部工厂类"""

#     @staticmethod
#     def new_cartesian_point(x, y):
#         """从笛卡尔坐标 (x, y) 创建点"""
#         return Point(x, y)

#     @staticmethod
#     def new_polar_point(r, theta):
#         """从极坐标 (r, θ) 创建点，自动转换为笛卡尔坐标"""
#         return Point(r * cos(theta), r * sin(theta))

if __name__ == "__main__":
    # 三种调用方式对比：
    # p1 = Point(1, 2)                              # 直接构造（需知道是笛卡尔坐标）
    # p2 = Point.PointFactory().new_polar_point(1, 2)  # 每次创建新工厂（浪费）
    
    # ✅ 推荐方式：使用共享的工厂实例
    p1 = Point.factory.new_cartesian_point(1, 2)
    p2 = Point.factory.new_polar_point(1, 2)
    print(p1)  # Point(x=1, y=2)
    print(p2)  # Point(x=-0.416..., y=0.909...)

# =============================================================================
# 抽象工厂模式 (Abstract Factory Pattern)
# =============================================================================
# 核心思想：
#   1. 定义抽象产品接口和抽象工厂接口
#   2. 每种具体产品都有对应的具体工厂
#   3. 客户端只依赖抽象接口，不关心具体实现
#   4. 新增产品类型只需添加新的产品类和工厂类，无需修改现有代码

# 结构图
# ┌─────────────────────────────────────────────────────────┐
# │                    HotDrinkMachine                      │
# │                      (客户端)                            │
# └────────────────────────┬────────────────────────────────┘
#                          │ 使用
#                          ▼
# ┌─────────────────────────────────────────────────────────┐
# │              HotDrinkFactory (抽象工厂)                  │
# │                  + prepare(amount)                      │
# └────────────────────────┬────────────────────────────────┘
#                          │ 实现
#           ┌──────────────┴──────────────┐
#           ▼                             ▼
#    ┌─────────────┐               ┌─────────────┐
#    │ TeaFactory  │               │CoffeeFactory│
#    │ prepare()   │               │ prepare()   │
#    └──────┬──────┘               └──────┬──────┘
#           │ 创建                        │ 创建
#           ▼                             ▼
#    ┌─────────────┐               ┌─────────────┐
#    │    Tea      │               │   Coffee    │
#    └─────────────┘               └─────────────┘
# =============================================================================

from abc import ABC, abstractmethod
from enum import Enum


# =============================================================================
# 抽象产品 + 具体产品
# =============================================================================

class HotDrink(ABC):
    """抽象产品：定义所有热饮必须实现的接口"""
    @abstractmethod
    def consume(self):
        pass


class Tea(HotDrink):
    """具体产品：茶"""
    def consume(self):
        print("This tea is delicious")


class Coffee(HotDrink):
    """具体产品：咖啡"""
    def consume(self):
        print("This coffee is delicious")


# =============================================================================
# 抽象工厂 + 具体工厂
# =============================================================================

class HotDrinkFactory(ABC):
    """
    抽象工厂：定义创建产品的接口
    所有具体工厂必须实现 prepare() 方法
    """
    @abstractmethod
    def prepare(self, amount):
        pass


class TeaFactory(HotDrinkFactory):
    """具体工厂：负责创建 Tea 产品"""
    def prepare(self, amount):
        print(f"Preparing {amount}ml of tea")
        return Tea()  # 返回具体产品


class CoffeeFactory(HotDrinkFactory):
    """具体工厂：负责创建 Coffee 产品"""
    def prepare(self, amount):
        print(f"Preparing {amount}ml of coffee")
        return Coffee()  # 返回具体产品


# =============================================================================
# 客户端：热饮机（使用工厂创建产品）
# =============================================================================

class HotDrinkMachine:
    """
    热饮机：作为客户端，通过工厂创建饮品
    特点：不直接创建 Tea/Coffee，而是通过对应的工厂
    """

    class AvailableDrink(Enum):
        """可用饮品枚举：新增饮品只需在此添加"""
        TEA = "tea"
        COFFEE = "coffee"

    # -------------------------------------------------------------------------
    # 动态工厂发现机制
    # -------------------------------------------------------------------------
    # 根据枚举名自动查找对应的工厂类（如 TEA -> TeaFactory）
    # 优势：新增饮品时只需：1. 添加枚举值  2. 创建对应的工厂类
    # -------------------------------------------------------------------------
    factories = []
    for d in AvailableDrink:
        factory_name = d.name.capitalize()              # TEA -> Tea
        # 动态工厂发现：通过 globals() 获取工厂类（如 TeaFactory）
        factory_instance = globals()[factory_name + "Factory"]()  # TeaFactory()
        factories.append((d, factory_instance))

    def make_drink(self):
        """制作饮品：展示菜单 -> 用户选择 -> 工厂生产"""
        print("Available drinks:")
        for idx, (drink, factory) in enumerate(self.factories):
            print(f"{idx}: {drink.name}")

        choice = int(input("Enter drink number: "))
        amount = int(input("Enter amount: "))

        # 关键点：通过工厂的 prepare() 创建产品，而非直接 new
        return self.factories[choice][1].prepare(amount)


if __name__ == "__main__":
    machine = HotDrinkMachine()
    drink = machine.make_drink()  # 返回 HotDrink（不知道具体是 Tea 还是 Coffee）
    drink.consume()               # 多态调用

```

## Portotype
- A partially or fully initialized object that you copy (clone) and make use of.
    - To implement a prototype, partially construct an object and store it somewhere
    - Deep copy the prototype
    - Customize the resulting instance
    - A factory provides a convenient API for using prototypes
```python
# =============================================================================
# 原型模式 (Prototype Pattern)
# =============================================================================
# 核心思想：
#   1. 通过复制现有对象（原型）来创建新对象，而非从头构造
#   2. 使用 deepcopy 确保嵌套对象也被完整复制
#   3. 复制后只需修改差异部分，避免重复设置公共属性
#   4. 适用场景：对象初始化成本高，或大部分属性相同只有少量差异
# =============================================================================

import copy


class Address:
    """地址类：作为 Employee 的嵌套对象"""
    def __init__(self, street_address, suite, city):
        self.suite = suite
        self.city = city
        self.street_address = street_address

    def __str__(self):
        return f"{self.street_address}, Suite #{self.suite}, {self.city}"


class Employee:
    """员工类：包含嵌套的 Address 对象"""
    def __init__(self, name, address):
        self.name = name
        self.address = address  # 嵌套对象，浅拷贝会出问题！

    def __str__(self):
        return f"{self.name} works at {self.address}"


class EmployeeFactory:
    """
    员工工厂：使用原型模式创建员工
    预定义原型对象，通过深拷贝 + 定制来创建新实例
    """

    # -------------------------------------------------------------------------
    # 原型对象：预设公司不同办公室的员工模板
    # -------------------------------------------------------------------------
    # 这些对象作为"模板"，包含固定的公司地址信息
    # 创建新员工时只需复制模板，然后填入个人信息
    # -------------------------------------------------------------------------
    main_office_employee = Employee(
        name="",
        address=Address(street_address="123 East Dr", suite=0, city="London")
    )
    aux_office_employee = Employee(
        name="",
        address=Address(street_address="123B East Dr", suite=0, city="London")
    )

    @staticmethod
    def __new_employee(proto, name, suite):
        """
        核心方法：从原型创建新员工

        关键点：必须用 deepcopy！
        - 浅拷贝 (copy.copy)：只复制顶层对象，嵌套的 Address 仍是同一个引用
        - 深拷贝 (copy.deepcopy)：递归复制所有嵌套对象，完全独立
        """
        result = copy.deepcopy(proto)  # ← 深拷贝原型
        result.name = name             # 定制：设置员工姓名
        result.address.suite = suite   # 定制：设置房间号
        return result

    @staticmethod
    def new_main_office_employee(name, suite):
        """工厂方法：创建主办公室员工"""
        return EmployeeFactory.__new_employee(
            EmployeeFactory.main_office_employee, name, suite
        )

    @staticmethod
    def new_aux_office_employee(name, suite):
        """工厂方法：创建分办公室员工"""
        return EmployeeFactory.__new_employee(
            EmployeeFactory.aux_office_employee, name, suite
        )


if __name__ == "__main__":
    # 创建两个不同办公室的员工
    john = EmployeeFactory.new_main_office_employee("John", 101)
    jane = EmployeeFactory.new_aux_office_employee("Jane", 102)

    print(john)  # John works at 123 East Dr, Suite #101, London
    print(jane)  # Jane works at 123B East Dr, Suite #102, London

# 为什么必须用 deepcopy
浅拷贝 (copy.copy)                    深拷贝 (copy.deepcopy)
┌─────────────┐                      ┌─────────────┐
│  john       │                      │  john       │
│  name: John │                      │  name: John │
│  address: ──┼──┐                   │  address: ──┼──► Address(101)
└─────────────┘  │                   └─────────────┘
                 ▼
┌─────────────┐  ┌──────────┐        ┌─────────────┐
│  jane       │  │ Address  │        │  jane       │
│  name: Jane │  │ suite:102│        │  name: Jane │
│  address: ──┼──┘ (共享!)  │        │  address: ──┼──► Address(102)
└─────────────┘  └──────────┘        └─────────────┘
❌ 修改 jane 会影响 john!            ✅ 完全独立

# 原型模式 vs 普通构造
# ❌ 普通构造：每次都要写完整地址
john = Employee("John", Address("123 East Dr", 101, "London"))
jane = Employee("Jane", Address("123 East Dr", 102, "London"))  # 重复！

# ✅ 原型模式：复制模板 + 定制差异
john = EmployeeFactory.new_main_office_employee("John", 101)
jane = EmployeeFactory.new_main_office_employee("Jane", 102)
```

## Singleton
- A component which is instantiated only once.

- Motivation
    - For some components it only makes sense to have one in the system, such as Database repository
    - the initializer call is expensive
        - We only do it once
        - We provide everyone with the same instance
        - Want to prevent anyone creating additional copies
        - Need to take care of lazy instantiation

```python
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