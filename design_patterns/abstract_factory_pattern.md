# Abstract Factory Pattern (抽象工厂模式)

## ASCII Diagram

```
+-------------------------+         +-------------------------+
|    AbstractFactory      |         |    AbstractProductA     |
+-------------------------+         +-------------------------+
| +createProductA()       |         | + operationA()          |
| +createProductB()       |         +-------------------------+
+-------------------------+                    ^
            ^                                  |
            |                    +-------------+-------------+
   +--------+--------+           |                           |
   |                 |   +----------------+         +----------------+
+------------------+ |   | ConcreteProductA1|       | ConcreteProductA2|
| ConcreteFactory1 | |   +----------------+         +----------------+
+------------------+ |   | + operationA() |         | + operationA() |
| +createProductA()|---->+----------------+         +----------------+
| +createProductB()|--+
+------------------+  |  +-------------------------+
                      |  |    AbstractProductB     |
+------------------+  |  +-------------------------+
| ConcreteFactory2 |  |  | + operationB()          |
+------------------+  |  +-------------------------+
| +createProductA()|  |             ^
| +createProductB()|--+             |
+------------------+  |  +----------+----------+
                      |  |                     |
                      |  +----------------+  +----------------+
                      +->| ConcreteProductB1|  | ConcreteProductB2|
                         +----------------+  +----------------+
                         | + operationB() |  | + operationB() |
                         +----------------+  +----------------+
```

**中文说明：**
- **AbstractFactory（抽象工厂）**：声明创建一系列相关产品的接口
- **ConcreteFactory（具体工厂）**：实现抽象工厂，创建具体产品
- **AbstractProduct（抽象产品）**：为一类产品声明接口
- **ConcreteProduct（具体产品）**：实现抽象产品接口的具体类
- **关键点**：一个工厂创建一系列相关的产品（产品族）

---

## 核心思想

提供一个接口，用于创建**一系列相关或相互依赖的对象**，而无需指定它们的具体类。抽象工厂模式强调的是产品族的概念，即一组相关的产品一起创建。

**与工厂方法的区别：**
- 工厂方法：创建一种产品
- 抽象工厂：创建一系列相关的产品（产品族）

---

## 应用场景

1. **产品族**：系统需要创建一组相关或相互依赖的产品
2. **多套产品体系**：系统有多于一个的产品族，但每次只使用其中一个
3. **约束产品匹配**：需要强制一组产品一起使用
4. **实际应用**：
   - GUI 工具包（Windows 风格、Mac 风格的按钮、输入框、菜单）
   - 数据库访问层（MySQL、PostgreSQL 的连接、命令、数据读取器）
   - 跨平台应用（不同操作系统的文件系统、网络、进程管理）
   - 游戏皮肤系统（不同主题的角色、武器、场景）

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 确保产品兼容 | 同一工厂创建的产品相互匹配 |
| 解耦客户端与具体类 | 客户端只使用抽象接口 |
| 符合开闭原则 | 新增产品族只需添加新工厂 |
| 易于切换产品族 | 只需更换工厂实例 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 新增产品类型困难 | 需要修改抽象工厂接口及所有实现 |
| 类数量膨胀 | 每个产品族需要一套完整的类 |
| 复杂度高 | 引入多层抽象 |

---

## Python 代码示例

### 应用前：直接创建对象

```python
# 问题：UI 组件直接实例化，不同风格混用导致界面不协调

class WindowsButton:
    def render(self):
        return "[Windows Button]"
    
    def click(self):
        return "Windows button clicked!"


class MacButton:
    def render(self):
        return "(Mac Button)"
    
    def click(self):
        return "Mac button clicked!"


class WindowsTextBox:
    def render(self):
        return "[Windows TextBox: ________]"
    
    def get_text(self):
        return "Text from Windows TextBox"


class MacTextBox:
    def render(self):
        return "(Mac TextBox: ________)"
    
    def get_text(self):
        return "Text from Mac TextBox"


class WindowsCheckbox:
    def render(self):
        return "[X] Windows Checkbox"
    
    def is_checked(self):
        return True


class MacCheckbox:
    def render(self):
        return "(✓) Mac Checkbox"
    
    def is_checked(self):
        return True


def create_login_form(style):
    """创建登录表单 - 直接依赖具体类"""
    
    if style == "windows":
        button = WindowsButton()
        username = WindowsTextBox()
        password = WindowsTextBox()
        remember = WindowsCheckbox()
    elif style == "mac":
        button = MacButton()
        username = MacTextBox()
        password = MacTextBox()
        remember = MacCheckbox()
    else:
        raise ValueError(f"Unknown style: {style}")
    
    print("=== Login Form ===")
    print(f"Username: {username.render()}")
    print(f"Password: {password.render()}")
    print(f"Remember: {remember.render()}")
    print(f"Submit: {button.render()}")


# 问题演示：可能不小心混用不同风格
def buggy_form():
    """Bug：不同风格混用"""
    button = WindowsButton()      # Windows 风格
    username = MacTextBox()       # Mac 风格 - 不匹配！
    checkbox = WindowsCheckbox()  # Windows 风格
    
    print(button.render())
    print(username.render())      # 界面风格不统一！
    print(checkbox.render())


create_login_form("windows")
# buggy_form()  # 这会导致界面风格不统一
```

### 应用后：使用抽象工厂模式

```python
from abc import ABC, abstractmethod


# ========== 抽象产品 ==========
class Button(ABC):
    """按钮抽象类"""
    
    @abstractmethod
    def render(self) -> str:
        pass
    
    @abstractmethod
    def click(self) -> str:
        pass


class TextBox(ABC):
    """文本框抽象类"""
    
    @abstractmethod
    def render(self) -> str:
        pass
    
    @abstractmethod
    def get_text(self) -> str:
        pass
    
    @abstractmethod
    def set_text(self, text: str) -> None:
        pass


class Checkbox(ABC):
    """复选框抽象类"""
    
    @abstractmethod
    def render(self) -> str:
        pass
    
    @abstractmethod
    def is_checked(self) -> bool:
        pass
    
    @abstractmethod
    def toggle(self) -> None:
        pass


# ========== Windows 产品族 ==========
class WindowsButton(Button):
    def render(self) -> str:
        return "┌─────────────┐\n│   Button    │\n└─────────────┘"
    
    def click(self) -> str:
        return "Windows button clicked with 3D effect!"


class WindowsTextBox(TextBox):
    def __init__(self):
        self._text = ""
    
    def render(self) -> str:
        return f"┌──────────────────┐\n│ {self._text:16} │\n└──────────────────┘"
    
    def get_text(self) -> str:
        return self._text
    
    def set_text(self, text: str) -> None:
        self._text = text


class WindowsCheckbox(Checkbox):
    def __init__(self):
        self._checked = False
    
    def render(self) -> str:
        mark = "X" if self._checked else " "
        return f"[{mark}] Checkbox"
    
    def is_checked(self) -> bool:
        return self._checked
    
    def toggle(self) -> None:
        self._checked = not self._checked


# ========== Mac 产品族 ==========
class MacButton(Button):
    def render(self) -> str:
        return "╭─────────────╮\n│   Button    │\n╰─────────────╯"
    
    def click(self) -> str:
        return "Mac button clicked with smooth animation!"


class MacTextBox(TextBox):
    def __init__(self):
        self._text = ""
    
    def render(self) -> str:
        return f"╭──────────────────╮\n│ {self._text:16} │\n╰──────────────────╯"
    
    def get_text(self) -> str:
        return self._text
    
    def set_text(self, text: str) -> None:
        self._text = text


class MacCheckbox(Checkbox):
    def __init__(self):
        self._checked = False
    
    def render(self) -> str:
        mark = "✓" if self._checked else " "
        return f"({mark}) Checkbox"
    
    def is_checked(self) -> bool:
        return self._checked
    
    def toggle(self) -> None:
        self._checked = not self._checked


# ========== Linux 产品族（新增，无需修改现有代码） ==========
class LinuxButton(Button):
    def render(self) -> str:
        return "+-------------+\n|   Button    |\n+-------------+"
    
    def click(self) -> str:
        return "Linux button clicked!"


class LinuxTextBox(TextBox):
    def __init__(self):
        self._text = ""
    
    def render(self) -> str:
        return f"+------------------+\n| {self._text:16} |\n+------------------+"
    
    def get_text(self) -> str:
        return self._text
    
    def set_text(self, text: str) -> None:
        self._text = text


class LinuxCheckbox(Checkbox):
    def __init__(self):
        self._checked = False
    
    def render(self) -> str:
        mark = "x" if self._checked else " "
        return f"<{mark}> Checkbox"
    
    def is_checked(self) -> bool:
        return self._checked
    
    def toggle(self) -> None:
        self._checked = not self._checked


# ========== 抽象工厂 ==========
class GUIFactory(ABC):
    """GUI 工厂抽象类 - 定义创建一系列相关产品的接口"""
    
    @abstractmethod
    def create_button(self) -> Button:
        pass
    
    @abstractmethod
    def create_textbox(self) -> TextBox:
        pass
    
    @abstractmethod
    def create_checkbox(self) -> Checkbox:
        pass


# ========== 具体工厂 ==========
class WindowsFactory(GUIFactory):
    """Windows GUI 工厂 - 创建 Windows 风格的完整组件族"""
    
    def create_button(self) -> Button:
        return WindowsButton()
    
    def create_textbox(self) -> TextBox:
        return WindowsTextBox()
    
    def create_checkbox(self) -> Checkbox:
        return WindowsCheckbox()


class MacFactory(GUIFactory):
    """Mac GUI 工厂 - 创建 Mac 风格的完整组件族"""
    
    def create_button(self) -> Button:
        return MacButton()
    
    def create_textbox(self) -> TextBox:
        return MacTextBox()
    
    def create_checkbox(self) -> Checkbox:
        return MacCheckbox()


class LinuxFactory(GUIFactory):
    """Linux GUI 工厂 - 创建 Linux 风格的完整组件族"""
    
    def create_button(self) -> Button:
        return LinuxButton()
    
    def create_textbox(self) -> TextBox:
        return LinuxTextBox()
    
    def create_checkbox(self) -> Checkbox:
        return LinuxCheckbox()


# ========== 客户端代码 ==========
class LoginForm:
    """登录表单 - 只依赖抽象接口"""
    
    def __init__(self, factory: GUIFactory):
        # 使用工厂创建所有组件 - 保证风格一致
        self.username_box = factory.create_textbox()
        self.password_box = factory.create_textbox()
        self.remember_checkbox = factory.create_checkbox()
        self.submit_button = factory.create_button()
    
    def render(self):
        print("╔══════════════════════════╗")
        print("║       LOGIN FORM         ║")
        print("╠══════════════════════════╣")
        print("║ Username:                ║")
        print(self.username_box.render())
        print("║ Password:                ║")
        print(self.password_box.render())
        print("║ Remember me:             ║")
        print(self.remember_checkbox.render())
        print("║                          ║")
        print(self.submit_button.render())
        print("╚══════════════════════════╝")
    
    def submit(self):
        username = self.username_box.get_text()
        remember = self.remember_checkbox.is_checked()
        print(self.submit_button.click())
        print(f"Logging in as: {username}, Remember: {remember}")


def get_factory_for_os(os_name: str) -> GUIFactory:
    """根据操作系统返回对应的工厂"""
    factories = {
        "windows": WindowsFactory,
        "mac": MacFactory,
        "linux": LinuxFactory,
    }
    factory_class = factories.get(os_name.lower())
    if not factory_class:
        raise ValueError(f"Unsupported OS: {os_name}")
    return factory_class()


# ========== 使用示例 ==========
if __name__ == "__main__":
    import platform
    
    # 可以根据实际操作系统选择工厂
    # os_name = platform.system().lower()
    
    # 演示不同风格
    for os_name in ["Windows", "Mac", "Linux"]:
        print(f"\n{'='*40}")
        print(f"  {os_name} Style Login Form")
        print(f"{'='*40}\n")
        
        factory = get_factory_for_os(os_name)
        login_form = LoginForm(factory)
        
        login_form.username_box.set_text("user@example.com")
        login_form.password_box.set_text("********")
        login_form.remember_checkbox.toggle()
        
        login_form.render()
        print()
        login_form.submit()
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **产品一致性** | 可能混用不同风格的组件 | 工厂保证创建的组件风格一致 |
| **扩展产品族** | 需要修改所有使用组件的代码 | 只需新增一个工厂类 |
| **客户端耦合** | 客户端依赖所有具体产品类 | 客户端只依赖抽象接口 |
| **切换风格** | 需要修改多处实例化代码 | 只需更换工厂实例 |
| **代码复用** | 创建逻辑分散在各处 | 创建逻辑集中在工厂中 |

---

## 与其他模式的关系

```
+-------------------+     +-------------------+     +-------------------+
|  Simple Factory   |     | Factory Method    |     | Abstract Factory  |
+-------------------+     +-------------------+     +-------------------+
| - 一个工厂类       |     | - 一个产品的创建   |     | - 一族产品的创建   |
| - 静态方法创建对象  |     | - 子类决定创建类型 |     | - 多个工厂方法     |
| - 不符合开闭原则   |     | - 符合开闭原则     |     | - 保证产品兼容     |
+-------------------+     +-------------------+     +-------------------+
         |                         |                         |
         +-----------+-------------+-----------+-------------+
                     |                         |
                  简单  <-------------------> 复杂
                  单一产品 <---------------> 产品族
```

---

## 实际应用：数据库访问层

```python
from abc import ABC, abstractmethod


# ========== 抽象产品 ==========
class Connection(ABC):
    @abstractmethod
    def connect(self) -> str:
        pass
    
    @abstractmethod
    def close(self) -> str:
        pass


class Command(ABC):
    @abstractmethod
    def execute(self, sql: str) -> str:
        pass


class DataReader(ABC):
    @abstractmethod
    def read(self) -> list:
        pass


# ========== MySQL 产品族 ==========
class MySQLConnection(Connection):
    def connect(self) -> str:
        return "Connected to MySQL database"
    
    def close(self) -> str:
        return "MySQL connection closed"


class MySQLCommand(Command):
    def execute(self, sql: str) -> str:
        return f"Executing MySQL query: {sql}"


class MySQLDataReader(DataReader):
    def read(self) -> list:
        return ["MySQL row 1", "MySQL row 2"]


# ========== PostgreSQL 产品族 ==========
class PostgreSQLConnection(Connection):
    def connect(self) -> str:
        return "Connected to PostgreSQL database"
    
    def close(self) -> str:
        return "PostgreSQL connection closed"


class PostgreSQLCommand(Command):
    def execute(self, sql: str) -> str:
        return f"Executing PostgreSQL query: {sql}"


class PostgreSQLDataReader(DataReader):
    def read(self) -> list:
        return ["PostgreSQL row 1", "PostgreSQL row 2"]


# ========== 抽象工厂 ==========
class DatabaseFactory(ABC):
    @abstractmethod
    def create_connection(self) -> Connection:
        pass
    
    @abstractmethod
    def create_command(self) -> Command:
        pass
    
    @abstractmethod
    def create_reader(self) -> DataReader:
        pass


# ========== 具体工厂 ==========
class MySQLFactory(DatabaseFactory):
    def create_connection(self) -> Connection:
        return MySQLConnection()
    
    def create_command(self) -> Command:
        return MySQLCommand()
    
    def create_reader(self) -> DataReader:
        return MySQLDataReader()


class PostgreSQLFactory(DatabaseFactory):
    def create_connection(self) -> Connection:
        return PostgreSQLConnection()
    
    def create_command(self) -> Command:
        return PostgreSQLCommand()
    
    def create_reader(self) -> DataReader:
        return PostgreSQLDataReader()


# ========== 使用 ==========
def query_users(factory: DatabaseFactory):
    conn = factory.create_connection()
    cmd = factory.create_command()
    reader = factory.create_reader()
    
    print(conn.connect())
    print(cmd.execute("SELECT * FROM users"))
    print(f"Results: {reader.read()}")
    print(conn.close())


# 切换数据库只需更换工厂
query_users(MySQLFactory())
print()
query_users(PostgreSQLFactory())
```

