# Abstract Factory Pattern in Python

---

## 1. Pattern Name: Abstract Factory

**Purpose / Problem Solved:**
Provide an interface for creating **families of related objects** without specifying their concrete classes. Ensures that products from the same family are used together consistently.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
|------------------------------------------------------------------|
| Uses only abstract interfaces:                                    |
|   - AbstractFactory                                               |
|   - AbstractProductA, AbstractProductB                            |
+------------------------------------------------------------------+
                              |
                              | Receives factory instance
                              v
+------------------------------------------------------------------+
|                    ABSTRACT FACTORY                               |
|------------------------------------------------------------------|
| + create_product_a() -> AbstractProductA                          |
| + create_product_b() -> AbstractProductB                          |
+------------------------------------------------------------------+
         ^                                    ^
         |                                    |
+---------------------+             +---------------------+
|  ConcreteFactory1   |             |  ConcreteFactory2   |
|---------------------|             |---------------------|
| create_product_a()  |             | create_product_a()  |
|   -> ProductA1      |             |   -> ProductA2      |
| create_product_b()  |             | create_product_b()  |
|   -> ProductB1      |             |   -> ProductB2      |
+---------------------+             +---------------------+
         |                                    |
         v                                    v
+------------------+                +------------------+
|    FAMILY 1      |                |    FAMILY 2      |
|------------------|                |------------------|
| ProductA1        |                | ProductA2        |
| ProductB1        |                | ProductB2        |
| (work together)  |                | (work together)  |
+------------------+                +------------------+

Example: GUI Toolkit Families
+------------------+                +------------------+
|   WINDOWS GUI    |                |    MACOS GUI     |
|------------------|                |------------------|
| WindowsButton    |                | MacOSButton      |
| WindowsCheckbox  |                | MacOSCheckbox    |
| WindowsMenu      |                | MacOSMenu        |
+------------------+                +------------------+
```

**中文说明：**
抽象工厂模式用于创建**相关对象家族**。与工厂方法创建单一产品不同，抽象工厂创建多个相互配合的产品。每个具体工厂生产一个完整的产品家族（如Windows风格或MacOS风格的全套UI组件）。客户端代码只使用抽象接口，可以在不修改代码的情况下切换整个产品家族。常用于跨平台UI、数据库驱动套件等场景。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Abstract Factory |
|----------------|----------------------------------|
| **Protocol classes** | Define product interfaces without inheritance |
| **abc.ABC** | Define abstract factory with required methods |
| **Module as factory** | Entire module can serve as a factory namespace |
| **Dict of factories** | Map names to factory objects for runtime selection |
| **Dataclasses** | Quick product class definitions |
| **Type hints** | `Factory[T]` generic factory types |
| **Enum for families** | Type-safe family selection |
| **`__init__.py`** | Package can expose family-specific factory |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Django database backends** | Each backend provides Connection, Cursor, Operations as a family |
| **Matplotlib backends** | Each backend (TkAgg, Qt5Agg) provides Figure, Canvas, Renderer |
| **SQLAlchemy dialects** | Each dialect provides TypeEngine, Compiler, DDLCompiler |
| **Pygame display modes** | Different display backends with compatible surfaces |
| **Pillow image formats** | Each format provides ImageFile, ImageFileDirectory |

```python
# Example concept from Django database backends
# Each backend is an abstract factory for database components

# sqlite3 backend family
from django.db.backends.sqlite3 import base
connection = base.DatabaseWrapper(settings_dict)
# Provides: cursor, operations, introspection, etc.

# postgresql backend family  
from django.db.backends.postgresql import base
connection = base.DatabaseWrapper(settings_dict)
# Provides same interface, different implementation
```

---

## 5. Python Module Examples

### Example 1: Cross-Platform UI Factory

```python
#!/usr/bin/env python3
"""
Abstract Factory Pattern - Cross-Platform UI Components

Creates families of UI components that work together.
Each platform (Windows, MacOS, Linux) has its own component family.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum, auto
from typing import Protocol


# ============== PRODUCT INTERFACES ==============

class Button(Protocol):
    """Abstract Button interface."""
    
    def render(self) -> str:
        """Render the button."""
        ...
    
    def click(self) -> str:
        """Handle click event."""
        ...


class Checkbox(Protocol):
    """Abstract Checkbox interface."""
    
    def render(self) -> str:
        """Render the checkbox."""
        ...
    
    def toggle(self) -> str:
        """Toggle checkbox state."""
        ...


class TextInput(Protocol):
    """Abstract TextInput interface."""
    
    def render(self) -> str:
        """Render the text input."""
        ...
    
    def set_value(self, value: str) -> str:
        """Set input value."""
        ...


# ============== WINDOWS FAMILY ==============

@dataclass
class WindowsButton:
    """Windows-style button."""
    label: str
    
    def render(self) -> str:
        return f"[====== {self.label} ======]"  # Windows-style flat button
    
    def click(self) -> str:
        return f"Windows button '{self.label}' clicked!"


@dataclass
class WindowsCheckbox:
    """Windows-style checkbox."""
    label: str
    checked: bool = False
    
    def render(self) -> str:
        mark = "☑" if self.checked else "☐"
        return f"{mark} {self.label}"
    
    def toggle(self) -> str:
        self.checked = not self.checked
        state = "checked" if self.checked else "unchecked"
        return f"Windows checkbox '{self.label}' is now {state}"


@dataclass
class WindowsTextInput:
    """Windows-style text input."""
    placeholder: str
    value: str = ""
    
    def render(self) -> str:
        display = self.value if self.value else self.placeholder
        return f"|{display:-^30}|"
    
    def set_value(self, value: str) -> str:
        self.value = value
        return f"Windows input value set to: {value}"


# ============== MACOS FAMILY ==============

@dataclass
class MacOSButton:
    """MacOS-style button."""
    label: str
    
    def render(self) -> str:
        return f"(  {self.label}  )"  # MacOS-style rounded button
    
    def click(self) -> str:
        return f"MacOS button '{self.label}' clicked with smooth animation!"


@dataclass
class MacOSCheckbox:
    """MacOS-style checkbox."""
    label: str
    checked: bool = False
    
    def render(self) -> str:
        mark = "✓" if self.checked else "○"
        return f"[{mark}] {self.label}"
    
    def toggle(self) -> str:
        self.checked = not self.checked
        state = "checked" if self.checked else "unchecked"
        return f"MacOS checkbox '{self.label}' is now {state} (with bounce)"


@dataclass
class MacOSTextInput:
    """MacOS-style text input."""
    placeholder: str
    value: str = ""
    
    def render(self) -> str:
        display = self.value if self.value else self.placeholder
        return f"⌈{display:^30}⌉"
    
    def set_value(self, value: str) -> str:
        self.value = value
        return f"MacOS input value set to: {value}"


# ============== LINUX FAMILY ==============

@dataclass
class LinuxButton:
    """Linux GTK-style button."""
    label: str
    
    def render(self) -> str:
        return f"<{self.label}>"  # GTK-style button
    
    def click(self) -> str:
        return f"Linux button '{self.label}' clicked (emitting signal)!"


@dataclass
class LinuxCheckbox:
    """Linux GTK-style checkbox."""
    label: str
    checked: bool = False
    
    def render(self) -> str:
        mark = "[x]" if self.checked else "[ ]"
        return f"{mark} {self.label}"
    
    def toggle(self) -> str:
        self.checked = not self.checked
        state = "checked" if self.checked else "unchecked"
        return f"Linux checkbox '{self.label}' is now {state}"


@dataclass
class LinuxTextInput:
    """Linux GTK-style text input."""
    placeholder: str
    value: str = ""
    
    def render(self) -> str:
        display = self.value if self.value else self.placeholder
        return f"[{display:^30}]"
    
    def set_value(self, value: str) -> str:
        self.value = value
        return f"Linux input value set to: {value}"


# ============== ABSTRACT FACTORY ==============

class GUIFactory(ABC):
    """
    Abstract Factory interface.
    
    Each concrete factory produces a family of related UI components.
    """
    
    @abstractmethod
    def create_button(self, label: str) -> Button:
        """Create a button."""
        pass
    
    @abstractmethod
    def create_checkbox(self, label: str) -> Checkbox:
        """Create a checkbox."""
        pass
    
    @abstractmethod
    def create_text_input(self, placeholder: str) -> TextInput:
        """Create a text input."""
        pass


class WindowsFactory(GUIFactory):
    """Factory for Windows UI components."""
    
    def create_button(self, label: str) -> Button:
        return WindowsButton(label)
    
    def create_checkbox(self, label: str) -> Checkbox:
        return WindowsCheckbox(label)
    
    def create_text_input(self, placeholder: str) -> TextInput:
        return WindowsTextInput(placeholder)


class MacOSFactory(GUIFactory):
    """Factory for MacOS UI components."""
    
    def create_button(self, label: str) -> Button:
        return MacOSButton(label)
    
    def create_checkbox(self, label: str) -> Checkbox:
        return MacOSCheckbox(label)
    
    def create_text_input(self, placeholder: str) -> TextInput:
        return MacOSTextInput(placeholder)


class LinuxFactory(GUIFactory):
    """Factory for Linux GTK UI components."""
    
    def create_button(self, label: str) -> Button:
        return LinuxButton(label)
    
    def create_checkbox(self, label: str) -> Checkbox:
        return LinuxCheckbox(label)
    
    def create_text_input(self, placeholder: str) -> TextInput:
        return LinuxTextInput(placeholder)


# ============== FACTORY PROVIDER ==============

class Platform(Enum):
    """Supported platforms."""
    WINDOWS = auto()
    MACOS = auto()
    LINUX = auto()


def get_factory(platform: Platform) -> GUIFactory:
    """Get the appropriate factory for the platform."""
    factories = {
        Platform.WINDOWS: WindowsFactory(),
        Platform.MACOS: MacOSFactory(),
        Platform.LINUX: LinuxFactory(),
    }
    return factories[platform]


def detect_platform() -> Platform:
    """Detect the current platform."""
    import sys
    if sys.platform.startswith("win"):
        return Platform.WINDOWS
    elif sys.platform == "darwin":
        return Platform.MACOS
    else:
        return Platform.LINUX


# ============== CLIENT CODE ==============

def create_login_form(factory: GUIFactory):
    """
    Client code that creates a login form.
    
    Works with ANY factory - doesn't know concrete types!
    """
    print("Creating Login Form:")
    print("-" * 40)
    
    # Create components using factory
    username_input = factory.create_text_input("Enter username")
    password_input = factory.create_text_input("Enter password")
    remember_me = factory.create_checkbox("Remember me")
    login_button = factory.create_button("Login")
    
    # Render the form
    print(f"Username: {username_input.render()}")
    print(f"Password: {password_input.render()}")
    print(f"          {remember_me.render()}")
    print(f"          {login_button.render()}")
    print()
    
    # Simulate interaction
    print("Simulating interaction:")
    print(f"  {username_input.set_value('alice')}")
    print(f"  {remember_me.toggle()}")
    print(f"  {login_button.click()}")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 50)
    print("Abstract Factory Pattern - Cross-Platform UI")
    print("=" * 50)
    print()
    
    # Create same form with different platform factories
    for platform in Platform:
        print(f"\n{'='*50}")
        print(f"Platform: {platform.name}")
        print("=" * 50)
        
        factory = get_factory(platform)
        create_login_form(factory)
    
    # Auto-detect current platform
    print(f"\n{'='*50}")
    print(f"Auto-detected platform: {detect_platform().name}")
    print("=" * 50)
    factory = get_factory(detect_platform())
    create_login_form(factory)
```

---

### Example 2: Document Export System

```python
#!/usr/bin/env python3
"""
Abstract Factory Pattern - Document Export System

Creates families of document components (paragraphs, tables, images)
that can be exported to different formats (HTML, Markdown, LaTeX).
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List


# ============== ABSTRACT PRODUCTS ==============

class Paragraph(ABC):
    """Abstract paragraph component."""
    
    @abstractmethod
    def render(self, text: str) -> str:
        """Render paragraph with given text."""
        pass


class Table(ABC):
    """Abstract table component."""
    
    @abstractmethod
    def render(self, headers: List[str], rows: List[List[str]]) -> str:
        """Render table with headers and rows."""
        pass


class Image(ABC):
    """Abstract image component."""
    
    @abstractmethod
    def render(self, src: str, alt: str) -> str:
        """Render image with source and alt text."""
        pass


class Heading(ABC):
    """Abstract heading component."""
    
    @abstractmethod
    def render(self, text: str, level: int = 1) -> str:
        """Render heading with given level (1-6)."""
        pass


# ============== HTML FAMILY ==============

class HtmlParagraph(Paragraph):
    def render(self, text: str) -> str:
        return f"<p>{text}</p>"


class HtmlTable(Table):
    def render(self, headers: List[str], rows: List[List[str]]) -> str:
        lines = ["<table>", "  <thead>", "    <tr>"]
        for h in headers:
            lines.append(f"      <th>{h}</th>")
        lines.extend(["    </tr>", "  </thead>", "  <tbody>"])
        for row in rows:
            lines.append("    <tr>")
            for cell in row:
                lines.append(f"      <td>{cell}</td>")
            lines.append("    </tr>")
        lines.extend(["  </tbody>", "</table>"])
        return "\n".join(lines)


class HtmlImage(Image):
    def render(self, src: str, alt: str) -> str:
        return f'<img src="{src}" alt="{alt}" />'


class HtmlHeading(Heading):
    def render(self, text: str, level: int = 1) -> str:
        return f"<h{level}>{text}</h{level}>"


# ============== MARKDOWN FAMILY ==============

class MarkdownParagraph(Paragraph):
    def render(self, text: str) -> str:
        return f"{text}\n"


class MarkdownTable(Table):
    def render(self, headers: List[str], rows: List[List[str]]) -> str:
        lines = []
        # Header row
        lines.append("| " + " | ".join(headers) + " |")
        # Separator
        lines.append("| " + " | ".join(["---"] * len(headers)) + " |")
        # Data rows
        for row in rows:
            lines.append("| " + " | ".join(row) + " |")
        return "\n".join(lines)


class MarkdownImage(Image):
    def render(self, src: str, alt: str) -> str:
        return f"![{alt}]({src})"


class MarkdownHeading(Heading):
    def render(self, text: str, level: int = 1) -> str:
        return "#" * level + " " + text


# ============== LATEX FAMILY ==============

class LatexParagraph(Paragraph):
    def render(self, text: str) -> str:
        return f"{text}\n\n"


class LatexTable(Table):
    def render(self, headers: List[str], rows: List[List[str]]) -> str:
        cols = "c" * len(headers)
        lines = [
            "\\begin{tabular}{" + cols + "}",
            "\\hline",
            " & ".join(headers) + " \\\\",
            "\\hline",
        ]
        for row in rows:
            lines.append(" & ".join(row) + " \\\\")
        lines.extend(["\\hline", "\\end{tabular}"])
        return "\n".join(lines)


class LatexImage(Image):
    def render(self, src: str, alt: str) -> str:
        return f"\\includegraphics{{{src}}} % {alt}"


class LatexHeading(Heading):
    def render(self, text: str, level: int = 1) -> str:
        commands = {
            1: "section",
            2: "subsection",
            3: "subsubsection",
            4: "paragraph",
            5: "subparagraph",
        }
        cmd = commands.get(level, "section")
        return f"\\{cmd}{{{text}}}"


# ============== ABSTRACT FACTORY ==============

class DocumentFactory(ABC):
    """Abstract factory for document components."""
    
    @abstractmethod
    def create_paragraph(self) -> Paragraph:
        pass
    
    @abstractmethod
    def create_table(self) -> Table:
        pass
    
    @abstractmethod
    def create_image(self) -> Image:
        pass
    
    @abstractmethod
    def create_heading(self) -> Heading:
        pass


class HtmlDocumentFactory(DocumentFactory):
    """Factory for HTML document components."""
    
    def create_paragraph(self) -> Paragraph:
        return HtmlParagraph()
    
    def create_table(self) -> Table:
        return HtmlTable()
    
    def create_image(self) -> Image:
        return HtmlImage()
    
    def create_heading(self) -> Heading:
        return HtmlHeading()


class MarkdownDocumentFactory(DocumentFactory):
    """Factory for Markdown document components."""
    
    def create_paragraph(self) -> Paragraph:
        return MarkdownParagraph()
    
    def create_table(self) -> Table:
        return MarkdownTable()
    
    def create_image(self) -> Image:
        return MarkdownImage()
    
    def create_heading(self) -> Heading:
        return MarkdownHeading()


class LatexDocumentFactory(DocumentFactory):
    """Factory for LaTeX document components."""
    
    def create_paragraph(self) -> Paragraph:
        return LatexParagraph()
    
    def create_table(self) -> Table:
        return LatexTable()
    
    def create_image(self) -> Image:
        return LatexImage()
    
    def create_heading(self) -> Heading:
        return LatexHeading()


# ============== DOCUMENT BUILDER (CLIENT) ==============

class DocumentBuilder:
    """
    Client that builds documents using a factory.
    
    Doesn't know about concrete component types.
    """
    
    def __init__(self, factory: DocumentFactory):
        self.factory = factory
        self.content: List[str] = []
    
    def add_heading(self, text: str, level: int = 1):
        heading = self.factory.create_heading()
        self.content.append(heading.render(text, level))
        return self
    
    def add_paragraph(self, text: str):
        paragraph = self.factory.create_paragraph()
        self.content.append(paragraph.render(text))
        return self
    
    def add_table(self, headers: List[str], rows: List[List[str]]):
        table = self.factory.create_table()
        self.content.append(table.render(headers, rows))
        return self
    
    def add_image(self, src: str, alt: str):
        image = self.factory.create_image()
        self.content.append(image.render(src, alt))
        return self
    
    def build(self) -> str:
        return "\n".join(self.content)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    # Sample data
    headers = ["Name", "Role", "Department"]
    rows = [
        ["Alice", "Developer", "Engineering"],
        ["Bob", "Designer", "Creative"],
        ["Carol", "Manager", "Operations"],
    ]
    
    # Build same document in different formats
    factories = {
        "HTML": HtmlDocumentFactory(),
        "Markdown": MarkdownDocumentFactory(),
        "LaTeX": LatexDocumentFactory(),
    }
    
    for format_name, factory in factories.items():
        print("=" * 60)
        print(f"Document in {format_name} format:")
        print("=" * 60)
        
        doc = (
            DocumentBuilder(factory)
            .add_heading("Team Directory")
            .add_paragraph("This document lists all team members.")
            .add_heading("Team Members", level=2)
            .add_table(headers, rows)
            .add_paragraph("For more info, see the organization chart below.")
            .add_image("org_chart.png", "Organization Chart")
            .build()
        )
        
        print(doc)
        print()
```

---

### Example 3: Database Driver Suite

```python
#!/usr/bin/env python3
"""
Abstract Factory Pattern - Database Driver Suite

Each database type provides a family of related components:
- Connection
- Cursor
- Query Builder

All components in a family are designed to work together.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, List, Dict, Optional


# ============== ABSTRACT PRODUCTS ==============

class Connection(ABC):
    """Abstract database connection."""
    
    @abstractmethod
    def connect(self) -> str:
        pass
    
    @abstractmethod
    def disconnect(self) -> str:
        pass
    
    @abstractmethod
    def is_connected(self) -> bool:
        pass


class Cursor(ABC):
    """Abstract database cursor."""
    
    @abstractmethod
    def execute(self, query: str) -> str:
        pass
    
    @abstractmethod
    def fetchall(self) -> List[Dict[str, Any]]:
        pass
    
    @abstractmethod
    def fetchone(self) -> Optional[Dict[str, Any]]:
        pass


class QueryBuilder(ABC):
    """Abstract SQL query builder."""
    
    @abstractmethod
    def select(self, *columns: str) -> "QueryBuilder":
        pass
    
    @abstractmethod
    def from_table(self, table: str) -> "QueryBuilder":
        pass
    
    @abstractmethod
    def where(self, condition: str) -> "QueryBuilder":
        pass
    
    @abstractmethod
    def build(self) -> str:
        pass


# ============== POSTGRESQL FAMILY ==============

@dataclass
class PostgresConnection(Connection):
    host: str
    port: int = 5432
    _connected: bool = False
    
    def connect(self) -> str:
        self._connected = True
        return f"Connected to PostgreSQL at {self.host}:{self.port}"
    
    def disconnect(self) -> str:
        self._connected = False
        return "Disconnected from PostgreSQL"
    
    def is_connected(self) -> bool:
        return self._connected


@dataclass
class PostgresCursor(Cursor):
    connection: PostgresConnection
    _results: List[Dict[str, Any]] = field(default_factory=list)
    
    def execute(self, query: str) -> str:
        # Simulate query execution
        self._results = [
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"},
        ]
        return f"PostgreSQL: Executed query with EXPLAIN ANALYZE"
    
    def fetchall(self) -> List[Dict[str, Any]]:
        return self._results
    
    def fetchone(self) -> Optional[Dict[str, Any]]:
        return self._results[0] if self._results else None


class PostgresQueryBuilder(QueryBuilder):
    def __init__(self):
        self._columns: List[str] = []
        self._table: str = ""
        self._conditions: List[str] = []
    
    def select(self, *columns: str) -> "QueryBuilder":
        self._columns = list(columns) if columns else ["*"]
        return self
    
    def from_table(self, table: str) -> "QueryBuilder":
        self._table = f'"{table}"'  # PostgreSQL uses double quotes
        return self
    
    def where(self, condition: str) -> "QueryBuilder":
        self._conditions.append(condition)
        return self
    
    def build(self) -> str:
        query = f"SELECT {', '.join(self._columns)} FROM {self._table}"
        if self._conditions:
            query += f" WHERE {' AND '.join(self._conditions)}"
        return query + ";"


# ============== MYSQL FAMILY ==============

@dataclass
class MySQLConnection(Connection):
    host: str
    port: int = 3306
    _connected: bool = False
    
    def connect(self) -> str:
        self._connected = True
        return f"Connected to MySQL at {self.host}:{self.port}"
    
    def disconnect(self) -> str:
        self._connected = False
        return "Disconnected from MySQL"
    
    def is_connected(self) -> bool:
        return self._connected


@dataclass
class MySQLCursor(Cursor):
    connection: MySQLConnection
    _results: List[Dict[str, Any]] = field(default_factory=list)
    
    def execute(self, query: str) -> str:
        self._results = [
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"},
        ]
        return f"MySQL: Executed query (buffered cursor)"
    
    def fetchall(self) -> List[Dict[str, Any]]:
        return self._results
    
    def fetchone(self) -> Optional[Dict[str, Any]]:
        return self._results[0] if self._results else None


class MySQLQueryBuilder(QueryBuilder):
    def __init__(self):
        self._columns: List[str] = []
        self._table: str = ""
        self._conditions: List[str] = []
    
    def select(self, *columns: str) -> "QueryBuilder":
        self._columns = list(columns) if columns else ["*"]
        return self
    
    def from_table(self, table: str) -> "QueryBuilder":
        self._table = f"`{table}`"  # MySQL uses backticks
        return self
    
    def where(self, condition: str) -> "QueryBuilder":
        self._conditions.append(condition)
        return self
    
    def build(self) -> str:
        query = f"SELECT {', '.join(self._columns)} FROM {self._table}"
        if self._conditions:
            query += f" WHERE {' AND '.join(self._conditions)}"
        return query + ";"


# ============== SQLITE FAMILY ==============

@dataclass
class SQLiteConnection(Connection):
    database: str
    _connected: bool = False
    
    def connect(self) -> str:
        self._connected = True
        return f"Connected to SQLite database: {self.database}"
    
    def disconnect(self) -> str:
        self._connected = False
        return "Disconnected from SQLite"
    
    def is_connected(self) -> bool:
        return self._connected


@dataclass
class SQLiteCursor(Cursor):
    connection: SQLiteConnection
    _results: List[Dict[str, Any]] = field(default_factory=list)
    
    def execute(self, query: str) -> str:
        self._results = [
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"},
        ]
        return f"SQLite: Executed query"
    
    def fetchall(self) -> List[Dict[str, Any]]:
        return self._results
    
    def fetchone(self) -> Optional[Dict[str, Any]]:
        return self._results[0] if self._results else None


class SQLiteQueryBuilder(QueryBuilder):
    def __init__(self):
        self._columns: List[str] = []
        self._table: str = ""
        self._conditions: List[str] = []
    
    def select(self, *columns: str) -> "QueryBuilder":
        self._columns = list(columns) if columns else ["*"]
        return self
    
    def from_table(self, table: str) -> "QueryBuilder":
        self._table = table  # SQLite doesn't need quoting
        return self
    
    def where(self, condition: str) -> "QueryBuilder":
        self._conditions.append(condition)
        return self
    
    def build(self) -> str:
        query = f"SELECT {', '.join(self._columns)} FROM {self._table}"
        if self._conditions:
            query += f" WHERE {' AND '.join(self._conditions)}"
        return query + ";"


# ============== ABSTRACT FACTORY ==============

class DatabaseFactory(ABC):
    """Abstract factory for database components."""
    
    @abstractmethod
    def create_connection(self, **kwargs) -> Connection:
        pass
    
    @abstractmethod
    def create_cursor(self, connection: Connection) -> Cursor:
        pass
    
    @abstractmethod
    def create_query_builder(self) -> QueryBuilder:
        pass


class PostgresFactory(DatabaseFactory):
    def create_connection(self, **kwargs) -> Connection:
        return PostgresConnection(**kwargs)
    
    def create_cursor(self, connection: Connection) -> Cursor:
        return PostgresCursor(connection)  # type: ignore
    
    def create_query_builder(self) -> QueryBuilder:
        return PostgresQueryBuilder()


class MySQLFactory(DatabaseFactory):
    def create_connection(self, **kwargs) -> Connection:
        return MySQLConnection(**kwargs)
    
    def create_cursor(self, connection: Connection) -> Cursor:
        return MySQLCursor(connection)  # type: ignore
    
    def create_query_builder(self) -> QueryBuilder:
        return MySQLQueryBuilder()


class SQLiteFactory(DatabaseFactory):
    def create_connection(self, **kwargs) -> Connection:
        return SQLiteConnection(**kwargs)
    
    def create_cursor(self, connection: Connection) -> Cursor:
        return SQLiteCursor(connection)  # type: ignore
    
    def create_query_builder(self) -> QueryBuilder:
        return SQLiteQueryBuilder()


# ============== CLIENT CODE ==============

class DatabaseManager:
    """
    Client that manages database operations.
    
    Works with any database factory.
    """
    
    def __init__(self, factory: DatabaseFactory, **connection_kwargs):
        self.factory = factory
        self.connection = factory.create_connection(**connection_kwargs)
        self.cursor: Optional[Cursor] = None
    
    def connect(self):
        print(self.connection.connect())
        self.cursor = self.factory.create_cursor(self.connection)
    
    def disconnect(self):
        print(self.connection.disconnect())
        self.cursor = None
    
    def query(self, table: str, *columns: str, where: str = None):
        """Execute a SELECT query using the query builder."""
        builder = self.factory.create_query_builder()
        builder.select(*columns).from_table(table)
        if where:
            builder.where(where)
        
        query = builder.build()
        print(f"Query: {query}")
        
        if self.cursor:
            print(self.cursor.execute(query))
            return self.cursor.fetchall()
        return []


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Abstract Factory Pattern - Database Driver Suite")
    print("=" * 60)
    
    # Test with different database factories
    configs = [
        ("PostgreSQL", PostgresFactory(), {"host": "localhost"}),
        ("MySQL", MySQLFactory(), {"host": "db.example.com"}),
        ("SQLite", SQLiteFactory(), {"database": "app.db"}),
    ]
    
    for name, factory, kwargs in configs:
        print(f"\n--- {name} ---")
        
        db = DatabaseManager(factory, **kwargs)
        db.connect()
        
        results = db.query("users", "id", "name", where="active = 1")
        print(f"Results: {results}")
        
        db.disconnect()
```

---

## 6. When to Use / When to Avoid

### Use When:
- System must work with **multiple families of related products**
- Products from the same family must be **used together**
- You want to enforce **consistency** within a product family
- Adding new families should be easy (Open/Closed Principle)

### Avoid When:
- You only have one product type (use Factory Method instead)
- Product families rarely change (overkill)
- Products don't have strong family relationships

### Python Idiom:
Use **modules as factories** for simplicity:

```python
# postgres.py
def create_connection(): ...
def create_cursor(conn): ...
def create_query_builder(): ...

# mysql.py
def create_connection(): ...
def create_cursor(conn): ...
def create_query_builder(): ...

# client.py
import postgres as db  # or: import mysql as db
conn = db.create_connection()
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Factory Method** | Abstract Factory uses Factory Methods to create products |
| **Builder** | Can be used with Abstract Factory to build complex products |
| **Prototype** | Alternative implementation - clone prototypes instead of creating |
| **Singleton** | Concrete factories often implemented as singletons |

