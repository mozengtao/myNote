# Factory Method Pattern (工厂方法模式)

## ASCII Diagram

```
+-------------------+                    +-------------------+
|      Creator      |<<abstract>>        |      Product      |<<interface>>
+-------------------+                    +-------------------+
| + factoryMethod() |<<abstract>>        | + operation()     |
| + someOperation() |                    +-------------------+
+-------------------+                             ^
         ^                                        |
         |                         +--------------+--------------+
+--------+--------+                |                             |
|                 |      +-----------------+           +-----------------+
| ConcreteCreatorA|      | ConcreteProductA|           | ConcreteProductB|
+-----------------+      +-----------------+           +-----------------+
| +factoryMethod()|----->| + operation()   |           | + operation()   |
+-----------------+      +-----------------+           +-----------------+
|                 |
| ConcreteCreatorB|
+-----------------+--------------------------------------^
| +factoryMethod()|
+-----------------+
```

**中文说明：**
- **Creator（抽象创建者）**：声明工厂方法，返回 Product 类型的对象
- **ConcreteCreator（具体创建者）**：重写工厂方法，返回具体产品实例
- **Product（抽象产品）**：定义产品的接口
- **ConcreteProduct（具体产品）**：实现产品接口的具体类

---

## 核心思想

定义一个创建对象的接口，但让**子类决定实例化哪个类**。工厂方法使一个类的实例化延迟到其子类。核心在于将对象的创建与使用分离。

---

## 应用场景

1. **不知道具体类型**：客户端不需要知道它所创建的对象的具体类
2. **子类指定创建对象**：希望由子类来指定创建的对象类型
3. **委托职责**：将创建对象的职责委托给多个帮助子类中的某一个
4. **实际应用**：
   - 日志记录器（文件日志、数据库日志、控制台日志）
   - 数据库连接（MySQL、PostgreSQL、SQLite）
   - 支付方式（支付宝、微信、银行卡）
   - 文档导出（PDF、Word、Excel）

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 解耦 | 将产品的创建与使用分离 |
| 符合开闭原则 | 新增产品只需添加新的具体创建者和产品类 |
| 符合单一职责原则 | 将创建产品的代码放在单独的类中 |
| 灵活性 | 可以在运行时决定创建哪种产品 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 类数量增加 | 每新增一个产品需要增加对应的创建者类 |
| 复杂度增加 | 引入了额外的抽象层 |

---

## Python 代码示例

### 应用前：直接创建对象

```python
# 问题：代码中直接实例化具体类，耦合度高

class PDFDocument:
    def create(self):
        return "Creating PDF document"
    
    def save(self, filename):
        return f"Saving PDF as {filename}.pdf"


class WordDocument:
    def create(self):
        return "Creating Word document"
    
    def save(self, filename):
        return f"Saving Word as {filename}.docx"


class ExcelDocument:
    def create(self):
        return "Creating Excel document"
    
    def save(self, filename):
        return f"Saving Excel as {filename}.xlsx"


def export_report(doc_type, data, filename):
    """导出报告 - 直接依赖具体类"""
    
    # 问题：每增加一种文档类型，需要修改这里的代码
    if doc_type == "pdf":
        doc = PDFDocument()
    elif doc_type == "word":
        doc = WordDocument()
    elif doc_type == "excel":
        doc = ExcelDocument()
    else:
        raise ValueError(f"Unknown document type: {doc_type}")
    
    print(doc.create())
    # 处理数据...
    print(doc.save(filename))


# 使用
export_report("pdf", {"title": "Report"}, "monthly_report")
export_report("word", {"title": "Report"}, "monthly_report")

# 问题：
# 1. export_report 函数与所有具体文档类紧耦合
# 2. 新增文档类型需要修改 export_report 函数
# 3. 违反开闭原则
```

### 应用后：使用工厂方法模式

```python
from abc import ABC, abstractmethod


# ========== 抽象产品 ==========
class Document(ABC):
    """文档抽象类"""
    
    @abstractmethod
    def create(self) -> str:
        pass
    
    @abstractmethod
    def save(self, filename: str) -> str:
        pass
    
    @abstractmethod
    def get_extension(self) -> str:
        pass


# ========== 具体产品 ==========
class PDFDocument(Document):
    def create(self) -> str:
        return "Creating PDF document with vector graphics support"
    
    def save(self, filename: str) -> str:
        return f"Saving PDF as {filename}.pdf"
    
    def get_extension(self) -> str:
        return ".pdf"


class WordDocument(Document):
    def create(self) -> str:
        return "Creating Word document with rich text formatting"
    
    def save(self, filename: str) -> str:
        return f"Saving Word as {filename}.docx"
    
    def get_extension(self) -> str:
        return ".docx"


class ExcelDocument(Document):
    def create(self) -> str:
        return "Creating Excel document with spreadsheet support"
    
    def save(self, filename: str) -> str:
        return f"Saving Excel as {filename}.xlsx"
    
    def get_extension(self) -> str:
        return ".xlsx"


class MarkdownDocument(Document):
    """新增的文档类型 - 无需修改现有代码"""
    
    def create(self) -> str:
        return "Creating Markdown document with plain text formatting"
    
    def save(self, filename: str) -> str:
        return f"Saving Markdown as {filename}.md"
    
    def get_extension(self) -> str:
        return ".md"


# ========== 抽象创建者 ==========
class DocumentCreator(ABC):
    """文档创建者抽象类"""
    
    @abstractmethod
    def factory_method(self) -> Document:
        """工厂方法 - 由子类实现"""
        pass
    
    def export_report(self, data: dict, filename: str) -> None:
        """
        使用工厂方法创建文档并导出
        注意：这个方法使用工厂方法，但不关心具体返回什么类型的文档
        """
        # 调用工厂方法获取产品
        document = self.factory_method()
        
        print(document.create())
        print(f"Processing data: {data}")
        print(document.save(filename))
        print(f"Report exported successfully as {filename}{document.get_extension()}")
        print("-" * 50)


# ========== 具体创建者 ==========
class PDFCreator(DocumentCreator):
    def factory_method(self) -> Document:
        return PDFDocument()


class WordCreator(DocumentCreator):
    def factory_method(self) -> Document:
        return WordDocument()


class ExcelCreator(DocumentCreator):
    def factory_method(self) -> Document:
        return ExcelDocument()


class MarkdownCreator(DocumentCreator):
    """新增的创建者 - 无需修改现有代码"""
    
    def factory_method(self) -> Document:
        return MarkdownDocument()


# ========== 客户端代码 ==========
def client_code(creator: DocumentCreator, data: dict, filename: str):
    """
    客户端代码只依赖抽象的 DocumentCreator
    不需要知道具体创建的是哪种文档
    """
    creator.export_report(data, filename)


# ========== 使用示例 ==========
if __name__ == "__main__":
    report_data = {"title": "Monthly Sales Report", "month": "October"}
    
    # 使用不同的创建者
    print("=== PDF Export ===")
    client_code(PDFCreator(), report_data, "sales_report")
    
    print("=== Word Export ===")
    client_code(WordCreator(), report_data, "sales_report")
    
    print("=== Excel Export ===")
    client_code(ExcelCreator(), report_data, "sales_report")
    
    print("=== Markdown Export ===")
    client_code(MarkdownCreator(), report_data, "sales_report")


# ========== 工厂注册表（可选增强） ==========
class DocumentCreatorRegistry:
    """工厂注册表 - 根据类型字符串获取对应的创建者"""
    
    _creators: dict[str, type[DocumentCreator]] = {}
    
    @classmethod
    def register(cls, doc_type: str, creator_class: type[DocumentCreator]):
        cls._creators[doc_type] = creator_class
    
    @classmethod
    def get_creator(cls, doc_type: str) -> DocumentCreator:
        creator_class = cls._creators.get(doc_type)
        if not creator_class:
            raise ValueError(f"Unknown document type: {doc_type}")
        return creator_class()


# 注册创建者
DocumentCreatorRegistry.register("pdf", PDFCreator)
DocumentCreatorRegistry.register("word", WordCreator)
DocumentCreatorRegistry.register("excel", ExcelCreator)
DocumentCreatorRegistry.register("markdown", MarkdownCreator)

# 使用注册表
creator = DocumentCreatorRegistry.get_creator("pdf")
creator.export_report({"title": "Test"}, "test_report")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **耦合度** | 客户端直接依赖所有具体产品类 | 客户端只依赖抽象接口 |
| **扩展性** | 新增产品需修改现有代码 | 新增产品只需添加新类，符合开闭原则 |
| **可测试性** | 难以 mock 具体类 | 可以轻松注入 mock 创建者 |
| **职责分离** | 创建逻辑和业务逻辑混在一起 | 创建逻辑封装在工厂类中 |
| **灵活性** | 编译时确定对象类型 | 运行时动态决定创建哪种对象 |

---

## 变体：参数化工厂方法

```python
from abc import ABC, abstractmethod
from enum import Enum


class LogLevel(Enum):
    DEBUG = "debug"
    INFO = "info"
    ERROR = "error"


class Logger(ABC):
    @abstractmethod
    def log(self, message: str) -> None:
        pass


class ConsoleLogger(Logger):
    def log(self, message: str) -> None:
        print(f"[CONSOLE] {message}")


class FileLogger(Logger):
    def __init__(self, filename: str):
        self.filename = filename
    
    def log(self, message: str) -> None:
        print(f"[FILE:{self.filename}] {message}")


class DatabaseLogger(Logger):
    def __init__(self, connection_string: str):
        self.connection_string = connection_string
    
    def log(self, message: str) -> None:
        print(f"[DB:{self.connection_string}] {message}")


class LoggerFactory:
    """参数化工厂方法"""
    
    @staticmethod
    def create_logger(logger_type: str, **kwargs) -> Logger:
        if logger_type == "console":
            return ConsoleLogger()
        elif logger_type == "file":
            return FileLogger(kwargs.get("filename", "app.log"))
        elif logger_type == "database":
            return DatabaseLogger(kwargs.get("connection_string", "localhost"))
        else:
            raise ValueError(f"Unknown logger type: {logger_type}")


# 使用
logger1 = LoggerFactory.create_logger("console")
logger1.log("Hello from console")

logger2 = LoggerFactory.create_logger("file", filename="errors.log")
logger2.log("Error occurred")

logger3 = LoggerFactory.create_logger("database", connection_string="mysql://localhost:3306")
logger3.log("User logged in")
```

