# Visitor Pattern (访问者模式)

## ASCII Diagram

```
+-------------------+          +-------------------+
|      Element      |          |      Visitor      |<<interface>>
+-------------------+          +-------------------+
| + accept(visitor) |<-------->| + visitElementA() |
+-------------------+          | + visitElementB() |
         ^                     +-------------------+
         |                              ^
    +----+----+                         |
    |         |               +---------+---------+
+---+----+ +--+-----+         |                   |
|ElementA| |ElementB|   +-----+------+    +-------+----+
+--------+ +--------+   |VisitorImpl1|    |VisitorImpl2|
|+accept()|+accept()|   +------------+    +------------+
+--------+ +--------+   |+visitElemA()|   |+visitElemA()|
                        |+visitElemB()|   |+visitElemB()|
                        +------------+    +------------+

Double Dispatch:
+----------+     accept(visitor)     +-----------+     visitElementA(this)    +----------+
| Element A|------------------------>|  Visitor  |<--------------------------|          |
+----------+                         +-----------+                            +----------+

Object Structure:
+------------------+
| ObjectStructure  |
+------------------+        +----------+
| - elements[]     |------->| Element  |
+------------------+        +----------+
| + accept(visitor)|              |
+------------------+        +-----+-----+
                            |           |
                       +----+---+  +----+---+
                       |Elem A  |  |Elem B  |
                       +--------+  +--------+
```

**中文说明：**
- **Visitor（访问者接口）**：为每种元素类声明一个访问方法
- **ConcreteVisitor（具体访问者）**：实现对每种元素的操作
- **Element（元素接口）**：声明 accept 方法接受访问者
- **ConcreteElement（具体元素）**：实现 accept，调用访问者对应方法
- **ObjectStructure（对象结构）**：包含元素集合，提供遍历接口

---

## 核心思想

将**操作**与**对象结构**分离。定义一个新的操作，而不改变被操作的元素类。访问者模式使用了**双分派**技术：先根据元素类型分派，再根据访问者类型分派。

**双分派（Double Dispatch）：**
1. element.accept(visitor) → 根据 element 类型分派
2. visitor.visitElement(this) → 根据 visitor 类型分派

---

## 应用场景

1. **对象结构稳定**：元素类很少变化，但经常需要添加新操作
2. **多种操作**：需要对同一对象结构执行多种不同操作
3. **操作集中管理**：希望将相关操作集中在一个类中
4. **实际应用**：
   - 编译器的 AST 处理（类型检查、代码生成）
   - 文档导出（HTML、PDF、Markdown）
   - 报表生成
   - 对象序列化

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 开闭原则 | 新增操作只需添加新访问者 |
| 单一职责 | 每个访问者负责一类操作 |
| 操作集中 | 相关操作在同一个访问者中 |
| 累积状态 | 访问者可以累积遍历过程中的状态 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 元素扩展困难 | 新增元素类需要修改所有访问者 |
| 破坏封装 | 元素需要暴露内部状态给访问者 |
| 复杂度高 | 双分派机制理解成本高 |

---

## Python 代码示例

### 应用前：操作分散在各元素类中

```python
# 问题：文档元素，每种操作都需要修改所有元素类

class Text:
    def __init__(self, content):
        self.content = content
    
    def render_html(self):
        return f"<p>{self.content}</p>"
    
    def render_markdown(self):
        return self.content
    
    def count_words(self):
        return len(self.content.split())
    
    # 新增操作需要修改这个类
    def render_latex(self):
        return self.content


class Image:
    def __init__(self, url, alt):
        self.url = url
        self.alt = alt
    
    def render_html(self):
        return f'<img src="{self.url}" alt="{self.alt}"/>'
    
    def render_markdown(self):
        return f"![{self.alt}]({self.url})"
    
    def count_words(self):
        return len(self.alt.split())
    
    # 新增操作需要修改这个类
    def render_latex(self):
        return f"\\includegraphics{{{self.url}}}"


class Link:
    def __init__(self, url, text):
        self.url = url
        self.text = text
    
    def render_html(self):
        return f'<a href="{self.url}">{self.text}</a>'
    
    def render_markdown(self):
        return f"[{self.text}]({self.url})"
    
    def count_words(self):
        return len(self.text.split())


# 问题：
# 1. 每个元素类包含所有操作
# 2. 新增操作需要修改所有元素类
# 3. 相关操作分散在各个类中
# 4. 违反单一职责和开闭原则
```

### 应用后：使用访问者模式

```python
from abc import ABC, abstractmethod
from typing import List, Any
from dataclasses import dataclass


# ========== 访问者接口 ==========
class DocumentVisitor(ABC):
    """文档访问者接口"""
    
    @abstractmethod
    def visit_text(self, element: 'Text') -> Any:
        pass
    
    @abstractmethod
    def visit_image(self, element: 'Image') -> Any:
        pass
    
    @abstractmethod
    def visit_link(self, element: 'Link') -> Any:
        pass
    
    @abstractmethod
    def visit_heading(self, element: 'Heading') -> Any:
        pass
    
    @abstractmethod
    def visit_code_block(self, element: 'CodeBlock') -> Any:
        pass


# ========== 元素接口 ==========
class DocumentElement(ABC):
    """文档元素接口"""
    
    @abstractmethod
    def accept(self, visitor: DocumentVisitor) -> Any:
        pass


# ========== 具体元素 ==========
@dataclass
class Text(DocumentElement):
    """文本元素"""
    content: str
    bold: bool = False
    italic: bool = False
    
    def accept(self, visitor: DocumentVisitor) -> Any:
        return visitor.visit_text(self)


@dataclass
class Image(DocumentElement):
    """图片元素"""
    url: str
    alt: str
    width: int = None
    height: int = None
    
    def accept(self, visitor: DocumentVisitor) -> Any:
        return visitor.visit_image(self)


@dataclass
class Link(DocumentElement):
    """链接元素"""
    url: str
    text: str
    
    def accept(self, visitor: DocumentVisitor) -> Any:
        return visitor.visit_link(self)


@dataclass
class Heading(DocumentElement):
    """标题元素"""
    text: str
    level: int = 1
    
    def accept(self, visitor: DocumentVisitor) -> Any:
        return visitor.visit_heading(self)


@dataclass
class CodeBlock(DocumentElement):
    """代码块元素"""
    code: str
    language: str = ""
    
    def accept(self, visitor: DocumentVisitor) -> Any:
        return visitor.visit_code_block(self)


# ========== 具体访问者 ==========
class HtmlRenderer(DocumentVisitor):
    """HTML 渲染访问者"""
    
    def visit_text(self, element: Text) -> str:
        content = element.content
        if element.bold:
            content = f"<strong>{content}</strong>"
        if element.italic:
            content = f"<em>{content}</em>"
        return f"<p>{content}</p>"
    
    def visit_image(self, element: Image) -> str:
        attrs = f'src="{element.url}" alt="{element.alt}"'
        if element.width:
            attrs += f' width="{element.width}"'
        if element.height:
            attrs += f' height="{element.height}"'
        return f"<img {attrs}/>"
    
    def visit_link(self, element: Link) -> str:
        return f'<a href="{element.url}">{element.text}</a>'
    
    def visit_heading(self, element: Heading) -> str:
        return f"<h{element.level}>{element.text}</h{element.level}>"
    
    def visit_code_block(self, element: CodeBlock) -> str:
        lang_class = f' class="language-{element.language}"' if element.language else ""
        return f"<pre><code{lang_class}>{element.code}</code></pre>"


class MarkdownRenderer(DocumentVisitor):
    """Markdown 渲染访问者"""
    
    def visit_text(self, element: Text) -> str:
        content = element.content
        if element.bold:
            content = f"**{content}**"
        if element.italic:
            content = f"*{content}*"
        return content
    
    def visit_image(self, element: Image) -> str:
        return f"![{element.alt}]({element.url})"
    
    def visit_link(self, element: Link) -> str:
        return f"[{element.text}]({element.url})"
    
    def visit_heading(self, element: Heading) -> str:
        return f"{'#' * element.level} {element.text}"
    
    def visit_code_block(self, element: CodeBlock) -> str:
        return f"```{element.language}\n{element.code}\n```"


class LatexRenderer(DocumentVisitor):
    """LaTeX 渲染访问者"""
    
    def visit_text(self, element: Text) -> str:
        content = element.content
        if element.bold:
            content = f"\\textbf{{{content}}}"
        if element.italic:
            content = f"\\textit{{{content}}}"
        return content
    
    def visit_image(self, element: Image) -> str:
        return f"\\includegraphics{{{element.url}}}"
    
    def visit_link(self, element: Link) -> str:
        return f"\\href{{{element.url}}}{{{element.text}}}"
    
    def visit_heading(self, element: Heading) -> str:
        levels = {1: "section", 2: "subsection", 3: "subsubsection"}
        cmd = levels.get(element.level, "paragraph")
        return f"\\{cmd}{{{element.text}}}"
    
    def visit_code_block(self, element: CodeBlock) -> str:
        return f"\\begin{{verbatim}}\n{element.code}\n\\end{{verbatim}}"


class WordCounter(DocumentVisitor):
    """字数统计访问者"""
    
    def __init__(self):
        self.total_words = 0
        self.total_chars = 0
        self.element_counts = {}
    
    def _count(self, text: str, element_type: str):
        words = len(text.split())
        chars = len(text)
        self.total_words += words
        self.total_chars += chars
        self.element_counts[element_type] = self.element_counts.get(element_type, 0) + 1
        return words
    
    def visit_text(self, element: Text) -> int:
        return self._count(element.content, "text")
    
    def visit_image(self, element: Image) -> int:
        return self._count(element.alt, "image")
    
    def visit_link(self, element: Link) -> int:
        return self._count(element.text, "link")
    
    def visit_heading(self, element: Heading) -> int:
        return self._count(element.text, "heading")
    
    def visit_code_block(self, element: CodeBlock) -> int:
        return self._count(element.code, "code")
    
    def get_stats(self) -> dict:
        return {
            "total_words": self.total_words,
            "total_chars": self.total_chars,
            "elements": self.element_counts
        }


class LinkExtractor(DocumentVisitor):
    """链接提取访问者"""
    
    def __init__(self):
        self.links: List[tuple] = []
        self.images: List[tuple] = []
    
    def visit_text(self, element: Text) -> None:
        pass
    
    def visit_image(self, element: Image) -> None:
        self.images.append((element.url, element.alt))
    
    def visit_link(self, element: Link) -> None:
        self.links.append((element.url, element.text))
    
    def visit_heading(self, element: Heading) -> None:
        pass
    
    def visit_code_block(self, element: CodeBlock) -> None:
        pass
    
    def get_results(self) -> dict:
        return {
            "links": self.links,
            "images": self.images
        }


# ========== 对象结构 ==========
class Document:
    """文档 - 对象结构"""
    
    def __init__(self):
        self._elements: List[DocumentElement] = []
    
    def add(self, element: DocumentElement):
        self._elements.append(element)
        return self
    
    def accept(self, visitor: DocumentVisitor) -> List[Any]:
        """让访问者访问所有元素"""
        results = []
        for element in self._elements:
            results.append(element.accept(visitor))
        return results
    
    def render(self, visitor: DocumentVisitor) -> str:
        """渲染文档"""
        return "\n".join(str(r) for r in self.accept(visitor))


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    # 创建文档
    doc = Document()
    doc.add(Heading("Welcome to My Document", level=1))
    doc.add(Text("This is a paragraph with ", bold=False))
    doc.add(Text("bold text", bold=True))
    doc.add(Text(" and ", italic=False))
    doc.add(Text("italic text", italic=True))
    doc.add(Heading("Code Example", level=2))
    doc.add(CodeBlock("def hello():\n    print('Hello')", language="python"))
    doc.add(Heading("Resources", level=2))
    doc.add(Link("https://python.org", "Python Official Site"))
    doc.add(Image("https://example.com/logo.png", "Logo"))
    
    print("=" * 60)
    print("HTML Output")
    print("=" * 60)
    print(doc.render(HtmlRenderer()))
    
    print("\n" + "=" * 60)
    print("Markdown Output")
    print("=" * 60)
    print(doc.render(MarkdownRenderer()))
    
    print("\n" + "=" * 60)
    print("LaTeX Output")
    print("=" * 60)
    print(doc.render(LatexRenderer()))
    
    print("\n" + "=" * 60)
    print("Statistics")
    print("=" * 60)
    counter = WordCounter()
    doc.accept(counter)
    print(f"Stats: {counter.get_stats()}")
    
    print("\n" + "=" * 60)
    print("Extracted Links")
    print("=" * 60)
    extractor = LinkExtractor()
    doc.accept(extractor)
    results = extractor.get_results()
    print(f"Links: {results['links']}")
    print(f"Images: {results['images']}")


# ========== AST 访问者示例 ==========
print("\n" + "=" * 60)
print("AST Visitor Example (Expression Evaluator)")
print("=" * 60)


class ExprVisitor(ABC):
    @abstractmethod
    def visit_number(self, expr: 'Number') -> Any:
        pass
    
    @abstractmethod
    def visit_binary_op(self, expr: 'BinaryOp') -> Any:
        pass
    
    @abstractmethod
    def visit_unary_op(self, expr: 'UnaryOp') -> Any:
        pass


class Expr(ABC):
    @abstractmethod
    def accept(self, visitor: ExprVisitor) -> Any:
        pass


@dataclass
class Number(Expr):
    value: float
    
    def accept(self, visitor: ExprVisitor) -> Any:
        return visitor.visit_number(self)


@dataclass
class BinaryOp(Expr):
    left: Expr
    op: str
    right: Expr
    
    def accept(self, visitor: ExprVisitor) -> Any:
        return visitor.visit_binary_op(self)


@dataclass
class UnaryOp(Expr):
    op: str
    operand: Expr
    
    def accept(self, visitor: ExprVisitor) -> Any:
        return visitor.visit_unary_op(self)


class Evaluator(ExprVisitor):
    """表达式求值访问者"""
    
    def visit_number(self, expr: Number) -> float:
        return expr.value
    
    def visit_binary_op(self, expr: BinaryOp) -> float:
        left = expr.left.accept(self)
        right = expr.right.accept(self)
        ops = {'+': lambda a, b: a + b, '-': lambda a, b: a - b,
               '*': lambda a, b: a * b, '/': lambda a, b: a / b}
        return ops[expr.op](left, right)
    
    def visit_unary_op(self, expr: UnaryOp) -> float:
        operand = expr.operand.accept(self)
        if expr.op == '-':
            return -operand
        return operand


class PrettyPrinter(ExprVisitor):
    """表达式打印访问者"""
    
    def visit_number(self, expr: Number) -> str:
        return str(expr.value)
    
    def visit_binary_op(self, expr: BinaryOp) -> str:
        left = expr.left.accept(self)
        right = expr.right.accept(self)
        return f"({left} {expr.op} {right})"
    
    def visit_unary_op(self, expr: UnaryOp) -> str:
        operand = expr.operand.accept(self)
        return f"({expr.op}{operand})"


# 构建表达式: -(3 + 4) * 2
expr = BinaryOp(
    UnaryOp('-', BinaryOp(Number(3), '+', Number(4))),
    '*',
    Number(2)
)

evaluator = Evaluator()
printer = PrettyPrinter()

print(f"Expression: {expr.accept(printer)}")
print(f"Result: {expr.accept(evaluator)}")
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **新增操作** | 修改所有元素类 | 只需添加新访问者 |
| **操作组织** | 操作分散在各元素中 | 相关操作集中在访问者中 |
| **单一职责** | 元素类承担多种操作 | 元素只负责数据，访问者负责操作 |
| **累积状态** | 难以在遍历中累积状态 | 访问者可以维护状态 |

---

## 何时使用访问者模式

```
适用:
✓ 对象结构稳定，很少添加新元素类
✓ 需要对元素执行多种不同操作
✓ 操作需要在遍历过程中累积状态

不适用:
✗ 经常需要添加新的元素类
✗ 只有少量操作
✗ 元素类层次简单
```

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **Composite** | 访问者常用于遍历组合结构 |
| **Iterator** | 可以用迭代器遍历，用访问者处理 |
| **Strategy** | 策略替换算法，访问者添加操作 |

