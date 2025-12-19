# Design Patterns - Behavioral Patterns Part 1 (行为型模式 第一部分)

A comprehensive guide to behavioral design patterns with English explanations,
Chinese details, ASCII diagrams, and Python code examples.

---

## Table of Contents

1. [Chain of Responsibility Pattern (责任链模式)](#1-chain-of-responsibility-pattern-责任链模式)
2. [Command Pattern (命令模式)](#2-command-pattern-命令模式)
3. [Iterator Pattern (迭代器模式)](#3-iterator-pattern-迭代器模式)
4. [Mediator Pattern (中介者模式)](#4-mediator-pattern-中介者模式)
5. [Memento Pattern (备忘录模式)](#5-memento-pattern-备忘录模式)
6. [Observer Pattern (观察者模式)](#6-observer-pattern-观察者模式)

---

## 1. Chain of Responsibility Pattern (责任链模式)

**Avoid coupling the sender of a request to its receiver by giving more than one object a chance to handle the request.**

### 中文详解

责任链模式是一种行为型设计模式，它允许你将请求沿着处理者链进行传递。收到请求后，每个处理者可以处理请求或将其传递给链上的下一个处理者。

**适用场景：**
- 当程序需要使用不同方式处理不同种类请求，而且请求类型和顺序预先未知时
- 当必须按顺序执行多个处理者时
- 当处理者及其顺序必须在运行时进行改变时
- 例如：日志级别过滤、权限验证链、事件处理

**优点：**
- 可以控制请求处理的顺序
- 单一职责原则：可以将发起操作和执行操作的类解耦
- 开闭原则：可以在不更改现有代码的情况下添加新的处理者

**缺点：**
- 部分请求可能未被处理

### Structure Diagram

```
+---------------+        +---------------+
|    Client     |        |    Handler    |
+---------------+        |  <<abstract>> |
       |                 +---------------+
       |                 | + handle()    |
       |                 | + set_next()  |
       +---------------->+---------------+
                                ^
                                |
         +----------------------+----------------------+
         |                      |                      |
+----------------+    +----------------+    +----------------+
| ConcreteHandler|    | ConcreteHandler|    | ConcreteHandler|
|       A        |    |       B        |    |       C        |
+----------------+    +----------------+    +----------------+
| - next_handler |--->| - next_handler |--->| - next_handler |
+----------------+    +----------------+    +----------------+
| + handle()     |    | + handle()     |    | + handle()     |
+----------------+    +----------------+    +----------------+

Request Flow:
Client --> [Handler A] --> [Handler B] --> [Handler C] --> (end or unhandled)
               |               |               |
               v               v               v
           (handle?)       (handle?)       (handle?)
```

**图解说明：**
- `Handler` 定义处理请求的接口和设置后继者的方法
- `ConcreteHandler` 处理它所负责的请求，或将请求转发给后继者
- Client 向链上的第一个 Handler 对象提交请求
- 请求沿链传递直到有一个 Handler 处理它

### Python Code Example

```python
"""
Chain of Responsibility Pattern Implementation in Python
责任链模式的 Python 实现

Example: Support ticket handling system
示例：技术支持工单处理系统
"""

from abc import ABC, abstractmethod
from typing import Optional, Dict, Any
from enum import Enum


class TicketPriority(Enum):
    LOW = 1
    MEDIUM = 2
    HIGH = 3
    CRITICAL = 4


class SupportTicket:
    """Represents a support ticket."""
    def __init__(self, title: str, description: str, 
                 priority: TicketPriority, category: str):
        self.title = title
        self.description = description
        self.priority = priority
        self.category = category
        self.handled_by: Optional[str] = None
        self.response: Optional[str] = None


class SupportHandler(ABC):
    """
    Abstract handler for support tickets.
    技术支持的抽象处理者。
    """
    def __init__(self):
        self._next_handler: Optional['SupportHandler'] = None

    def set_next(self, handler: 'SupportHandler') -> 'SupportHandler':
        """Set the next handler in the chain."""
        self._next_handler = handler
        return handler  # Allow chaining

    @abstractmethod
    def handle(self, ticket: SupportTicket) -> bool:
        """
        Handle the ticket or pass to next handler.
        Return True if handled, False if passed.
        """
        pass

    def pass_to_next(self, ticket: SupportTicket) -> bool:
        """Pass the ticket to the next handler."""
        if self._next_handler:
            return self._next_handler.handle(ticket)
        print(f"  [Chain End] No handler for ticket: {ticket.title}")
        return False


class FAQHandler(SupportHandler):
    """Handles simple FAQ-type questions."""
    
    FAQ_RESPONSES = {
        "password": "To reset your password, visit /reset-password",
        "account": "For account issues, please verify your email first",
        "billing": "Billing questions: contact billing@example.com",
    }

    def handle(self, ticket: SupportTicket) -> bool:
        # Check if it's a simple FAQ question
        for keyword, response in self.FAQ_RESPONSES.items():
            if keyword.lower() in ticket.description.lower():
                ticket.handled_by = "FAQ Bot"
                ticket.response = response
                print(f"  [FAQ Handler] Auto-resolved: {ticket.title}")
                return True

        print(f"  [FAQ Handler] Cannot handle, passing to next...")
        return self.pass_to_next(ticket)


class Level1SupportHandler(SupportHandler):
    """Handles low and medium priority tickets."""

    def handle(self, ticket: SupportTicket) -> bool:
        if ticket.priority in [TicketPriority.LOW, TicketPriority.MEDIUM]:
            ticket.handled_by = "Level 1 Support"
            ticket.response = f"Level 1 is working on: {ticket.title}"
            print(f"  [L1 Support] Handling: {ticket.title}")
            return True

        print(f"  [L1 Support] Priority too high, escalating...")
        return self.pass_to_next(ticket)


class Level2SupportHandler(SupportHandler):
    """Handles high priority tickets."""

    def handle(self, ticket: SupportTicket) -> bool:
        if ticket.priority == TicketPriority.HIGH:
            ticket.handled_by = "Level 2 Support (Senior)"
            ticket.response = f"Senior engineer assigned to: {ticket.title}"
            print(f"  [L2 Support] Handling: {ticket.title}")
            return True

        print(f"  [L2 Support] Critical issue, escalating to manager...")
        return self.pass_to_next(ticket)


class ManagerHandler(SupportHandler):
    """Handles critical priority tickets."""

    def handle(self, ticket: SupportTicket) -> bool:
        if ticket.priority == TicketPriority.CRITICAL:
            ticket.handled_by = "Support Manager"
            ticket.response = f"Manager directly handling: {ticket.title}"
            print(f"  [Manager] Handling critical: {ticket.title}")
            return True

        return self.pass_to_next(ticket)


class TicketSystem:
    """Client that uses the chain of responsibility."""
    
    def __init__(self):
        # Build the chain
        self.faq = FAQHandler()
        self.level1 = Level1SupportHandler()
        self.level2 = Level2SupportHandler()
        self.manager = ManagerHandler()

        # Set up the chain
        self.faq.set_next(self.level1).set_next(self.level2).set_next(self.manager)

    def submit_ticket(self, ticket: SupportTicket) -> None:
        """Submit a ticket to the support chain."""
        print(f"\nSubmitting: '{ticket.title}' (Priority: {ticket.priority.name})")
        handled = self.faq.handle(ticket)
        
        if handled:
            print(f"  Result: Handled by {ticket.handled_by}")
            print(f"  Response: {ticket.response}")
        else:
            print(f"  Result: Ticket could not be handled")


# Client code demonstration
if __name__ == "__main__":
    print("=== Chain of Responsibility Pattern Demo ===")

    system = TicketSystem()

    # Test tickets
    tickets = [
        SupportTicket("Password Reset", "How do I reset my password?",
                      TicketPriority.LOW, "account"),
        SupportTicket("Slow Performance", "The app is running slowly",
                      TicketPriority.MEDIUM, "technical"),
        SupportTicket("Data Loss", "I lost some important data",
                      TicketPriority.HIGH, "technical"),
        SupportTicket("System Down", "Production system is completely down!",
                      TicketPriority.CRITICAL, "outage"),
        SupportTicket("Feature Request", "Add dark mode please",
                      TicketPriority.LOW, "feature"),
    ]

    for ticket in tickets:
        system.submit_ticket(ticket)
```

---

## 2. Command Pattern (命令模式)

**Encapsulate a request as an object, thereby letting you parameterize clients with different requests, queue or log requests, and support undoable operations.**

### 中文详解

命令模式是一种行为型设计模式，它将请求封装为一个对象，从而让你可以使用不同的请求、队列或者日志请求来参数化其他对象，同时支持可撤消的操作。

**适用场景：**
- 当需要将请求发送者与请求接收者解耦时
- 当需要支持撤销/重做操作时
- 当需要支持事务时
- 当需要将操作放入队列或远程执行时
- 例如：GUI 操作、宏录制、事务系统

**优点：**
- 单一职责原则：解耦触发和执行操作的类
- 开闭原则：可以在不修改现有代码的情况下添加新命令
- 可以实现撤销/重做功能
- 可以实现延迟执行操作

**缺点：**
- 代码可能会变得更加复杂，因为需要在发送者和接收者之间引入新的层次

### Structure Diagram

```
+---------------+     +-------------------+
|    Invoker    |     |     Command       |
+---------------+     |   <<interface>>   |
| - command     |---->+-------------------+
+---------------+     | + execute()       |
| + set_command()|    | + undo()          |
| + execute_cmd()|    +-------------------+
+---------------+              ^
                               |
                    +----------+----------+
                    |                     |
           +----------------+    +----------------+
           | ConcreteCommand|    | ConcreteCommand|
           |       A        |    |       B        |
           +----------------+    +----------------+
           | - receiver     |    | - receiver     |
           | - state        |    | - state        |
           +----------------+    +----------------+
           | + execute()    |    | + execute()    |
           | + undo()       |    | + undo()       |
           +----------------+    +----------------+
                    |                     |
                    v                     v
           +----------------+    +----------------+
           |   Receiver A   |    |   Receiver B   |
           +----------------+    +----------------+
           | + action()     |    | + action()     |
           +----------------+    +----------------+
```

**图解说明：**
- `Command` 声明执行操作的接口
- `ConcreteCommand` 将接收者和一个动作绑定，调用接收者相应的操作
- `Receiver` 知道如何实施与执行请求相关的操作
- `Invoker` 要求该命令执行这个请求
- Client 创建 ConcreteCommand 并设定其接收者

### Python Code Example

```python
"""
Command Pattern Implementation in Python
命令模式的 Python 实现

Example: Text editor with undo/redo functionality
示例：具有撤销/重做功能的文本编辑器
"""

from abc import ABC, abstractmethod
from typing import List
from dataclasses import dataclass
from copy import deepcopy


# Receiver
class TextDocument:
    """
    Receiver: The actual text document.
    接收者：实际的文本文档。
    """
    def __init__(self):
        self._text = ""
        self._cursor_position = 0

    @property
    def text(self) -> str:
        return self._text

    @property
    def cursor(self) -> int:
        return self._cursor_position

    def insert(self, text: str, position: int) -> None:
        """Insert text at position."""
        self._text = self._text[:position] + text + self._text[position:]
        self._cursor_position = position + len(text)

    def delete(self, start: int, length: int) -> str:
        """Delete text and return deleted text."""
        deleted = self._text[start:start + length]
        self._text = self._text[:start] + self._text[start + length:]
        self._cursor_position = start
        return deleted

    def get_state(self) -> dict:
        """Get current state for undo."""
        return {"text": self._text, "cursor": self._cursor_position}

    def restore_state(self, state: dict) -> None:
        """Restore state from snapshot."""
        self._text = state["text"]
        self._cursor_position = state["cursor"]

    def __str__(self) -> str:
        # Show cursor position with |
        return f"{self._text[:self._cursor_position]}|{self._text[self._cursor_position:]}"


# Command interface
class Command(ABC):
    """
    Command interface.
    命令接口。
    """
    @abstractmethod
    def execute(self) -> None:
        pass

    @abstractmethod
    def undo(self) -> None:
        pass


# Concrete Commands
class InsertCommand(Command):
    """Command to insert text."""
    def __init__(self, document: TextDocument, text: str, position: int):
        self._document = document
        self._text = text
        self._position = position

    def execute(self) -> None:
        self._document.insert(self._text, self._position)

    def undo(self) -> None:
        self._document.delete(self._position, len(self._text))


class DeleteCommand(Command):
    """Command to delete text."""
    def __init__(self, document: TextDocument, start: int, length: int):
        self._document = document
        self._start = start
        self._length = length
        self._deleted_text = ""

    def execute(self) -> None:
        self._deleted_text = self._document.delete(self._start, self._length)

    def undo(self) -> None:
        self._document.insert(self._deleted_text, self._start)


class ReplaceCommand(Command):
    """Command to replace text (composite command)."""
    def __init__(self, document: TextDocument, start: int, 
                 length: int, new_text: str):
        self._document = document
        self._start = start
        self._length = length
        self._new_text = new_text
        self._old_text = ""

    def execute(self) -> None:
        self._old_text = self._document.delete(self._start, self._length)
        self._document.insert(self._new_text, self._start)

    def undo(self) -> None:
        self._document.delete(self._start, len(self._new_text))
        self._document.insert(self._old_text, self._start)


# Invoker
class TextEditor:
    """
    Invoker: Text editor with command history.
    调用者：具有命令历史的文本编辑器。
    """
    def __init__(self):
        self._document = TextDocument()
        self._history: List[Command] = []
        self._redo_stack: List[Command] = []

    @property
    def text(self) -> str:
        return self._document.text

    def execute_command(self, command: Command) -> None:
        """Execute a command and save to history."""
        command.execute()
        self._history.append(command)
        self._redo_stack.clear()  # Clear redo stack on new command

    def undo(self) -> bool:
        """Undo the last command."""
        if not self._history:
            return False
        command = self._history.pop()
        command.undo()
        self._redo_stack.append(command)
        return True

    def redo(self) -> bool:
        """Redo the last undone command."""
        if not self._redo_stack:
            return False
        command = self._redo_stack.pop()
        command.execute()
        self._history.append(command)
        return True

    def insert(self, text: str, position: int = None) -> None:
        """Insert text at position (or cursor)."""
        pos = position if position is not None else self._document.cursor
        command = InsertCommand(self._document, text, pos)
        self.execute_command(command)

    def delete(self, start: int, length: int) -> None:
        """Delete text."""
        command = DeleteCommand(self._document, start, length)
        self.execute_command(command)

    def replace(self, start: int, length: int, new_text: str) -> None:
        """Replace text."""
        command = ReplaceCommand(self._document, start, length, new_text)
        self.execute_command(command)

    def show(self) -> str:
        """Show current document state."""
        return str(self._document)

    def get_history_size(self) -> int:
        return len(self._history)


# Macro Command (composite pattern applied to commands)
class MacroCommand(Command):
    """
    Macro: A command that executes multiple commands.
    宏：执行多个命令的命令。
    """
    def __init__(self, commands: List[Command]):
        self._commands = commands

    def execute(self) -> None:
        for command in self._commands:
            command.execute()

    def undo(self) -> None:
        # Undo in reverse order
        for command in reversed(self._commands):
            command.undo()


# Client code demonstration
if __name__ == "__main__":
    print("=== Command Pattern Demo ===\n")

    editor = TextEditor()

    print("1. Basic operations:")
    editor.insert("Hello")
    print(f"   After insert 'Hello': {editor.show()}")

    editor.insert(" World")
    print(f"   After insert ' World': {editor.show()}")

    editor.insert("!")
    print(f"   After insert '!': {editor.show()}")
    print()

    print("2. Undo operations:")
    editor.undo()
    print(f"   After undo: {editor.show()}")

    editor.undo()
    print(f"   After undo: {editor.show()}")
    print()

    print("3. Redo operations:")
    editor.redo()
    print(f"   After redo: {editor.show()}")
    print()

    print("4. Delete operation:")
    editor.delete(0, 5)  # Delete "Hello"
    print(f"   After delete 'Hello': {editor.show()}")

    editor.undo()
    print(f"   After undo delete: {editor.show()}")
    print()

    print("5. Replace operation:")
    editor.replace(0, 5, "Hi")
    print(f"   After replace 'Hello' with 'Hi': {editor.show()}")

    editor.undo()
    print(f"   After undo replace: {editor.show()}")
    print()

    print(f"History size: {editor.get_history_size()} commands")
```

---

## 3. Iterator Pattern (迭代器模式)

**Provide a way to access the elements of an aggregate object sequentially without exposing its underlying representation.**

### 中文详解

迭代器模式是一种行为型设计模式，它让你能在不暴露集合底层表现形式（列表、栈、树等）的情况下遍历集合中所有的元素。

**适用场景：**
- 当集合背后有复杂的数据结构，但需要对客户端隐藏其复杂性时
- 当需要减少程序中重复的遍历代码时
- 当希望代码能够遍历不同的甚至是无法预知的数据结构时
- 例如：遍历树结构、数据库结果集、文件系统

**优点：**
- 单一职责原则：将庞大的遍历算法分解为单独的类
- 开闭原则：可以实现新类型的集合和迭代器而无需修改现有代码
- 可以并行遍历同一个集合
- 可以暂停遍历并在需要时继续

**缺点：**
- 如果程序只与简单的集合进行交互，可能会过度设计

### Structure Diagram

```
+-------------------+          +-------------------+
|     Iterator      |          |    Aggregate      |
|   <<interface>>   |          |   <<interface>>   |
+-------------------+          +-------------------+
| + first()         |          | + create_iterator()|
| + next()          |<---------+-------------------+
| + has_next()      |  creates           ^
| + current()       |                    |
+-------------------+          +---------+---------+
         ^                     |                   |
         |              +------------+      +------------+
         |              |ConcreteAgg |      |ConcreteAgg |
+-------------------+   |     A      |      |     B      |
|ConcreteIterator   |   +------------+      +------------+
+-------------------+   | - items[]  |      | - data     |
| - aggregate       |-->+------------+      +------------+
| - current_index   |   |+create_iter|      |+create_iter|
+-------------------+   +------------+      +------------+
| + first()         |
| + next()          |
| + has_next()      |
| + current()       |
+-------------------+
```

**图解说明：**
- `Iterator` 定义访问和遍历元素的接口
- `ConcreteIterator` 实现 Iterator 接口，跟踪遍历的当前位置
- `Aggregate` 定义创建 Iterator 对象的接口
- `ConcreteAggregate` 实现创建相应 Iterator 的接口
- Client 通过 Iterator 接口访问元素

### Python Code Example

```python
"""
Iterator Pattern Implementation in Python
迭代器模式的 Python 实现

Example: Custom collection with multiple iteration strategies
示例：具有多种迭代策略的自定义集合
"""

from abc import ABC, abstractmethod
from typing import Any, List, TypeVar, Generic
from collections.abc import Iterator, Iterable

T = TypeVar('T')


# Iterator interface
class TreeIterator(ABC, Generic[T]):
    """
    Abstract iterator for tree traversal.
    树遍历的抽象迭代器。
    """
    @abstractmethod
    def __next__(self) -> T:
        pass

    @abstractmethod
    def has_next(self) -> bool:
        pass

    def __iter__(self):
        return self


# Tree node
class TreeNode(Generic[T]):
    """A node in the tree."""
    def __init__(self, value: T):
        self.value = value
        self.left: 'TreeNode[T]' = None
        self.right: 'TreeNode[T]' = None


# Concrete Iterators
class InOrderIterator(TreeIterator[T]):
    """
    In-order traversal iterator (left, root, right).
    中序遍历迭代器（左、根、右）。
    """
    def __init__(self, root: TreeNode[T]):
        self._stack: List[TreeNode[T]] = []
        self._current = root
        self._push_left(root)

    def _push_left(self, node: TreeNode[T]) -> None:
        """Push all left nodes onto stack."""
        while node:
            self._stack.append(node)
            node = node.left

    def has_next(self) -> bool:
        return len(self._stack) > 0

    def __next__(self) -> T:
        if not self.has_next():
            raise StopIteration

        node = self._stack.pop()
        value = node.value

        if node.right:
            self._push_left(node.right)

        return value


class PreOrderIterator(TreeIterator[T]):
    """
    Pre-order traversal iterator (root, left, right).
    前序遍历迭代器（根、左、右）。
    """
    def __init__(self, root: TreeNode[T]):
        self._stack: List[TreeNode[T]] = []
        if root:
            self._stack.append(root)

    def has_next(self) -> bool:
        return len(self._stack) > 0

    def __next__(self) -> T:
        if not self.has_next():
            raise StopIteration

        node = self._stack.pop()
        value = node.value

        # Push right first so left is processed first
        if node.right:
            self._stack.append(node.right)
        if node.left:
            self._stack.append(node.left)

        return value


class PostOrderIterator(TreeIterator[T]):
    """
    Post-order traversal iterator (left, right, root).
    后序遍历迭代器（左、右、根）。
    """
    def __init__(self, root: TreeNode[T]):
        self._result: List[T] = []
        self._build_order(root)
        self._index = 0

    def _build_order(self, node: TreeNode[T]) -> None:
        if not node:
            return
        self._build_order(node.left)
        self._build_order(node.right)
        self._result.append(node.value)

    def has_next(self) -> bool:
        return self._index < len(self._result)

    def __next__(self) -> T:
        if not self.has_next():
            raise StopIteration
        value = self._result[self._index]
        self._index += 1
        return value


class LevelOrderIterator(TreeIterator[T]):
    """
    Level-order (BFS) traversal iterator.
    层序（广度优先）遍历迭代器。
    """
    def __init__(self, root: TreeNode[T]):
        from collections import deque
        self._queue = deque()
        if root:
            self._queue.append(root)

    def has_next(self) -> bool:
        return len(self._queue) > 0

    def __next__(self) -> T:
        if not self.has_next():
            raise StopIteration

        node = self._queue.popleft()
        value = node.value

        if node.left:
            self._queue.append(node.left)
        if node.right:
            self._queue.append(node.right)

        return value


# Aggregate (Collection)
class BinaryTree(Generic[T]):
    """
    Binary tree collection with multiple iteration strategies.
    具有多种迭代策略的二叉树集合。
    """
    def __init__(self):
        self._root: TreeNode[T] = None

    @property
    def root(self) -> TreeNode[T]:
        return self._root

    def insert(self, value: T) -> None:
        """Insert value into BST."""
        if not self._root:
            self._root = TreeNode(value)
        else:
            self._insert_recursive(self._root, value)

    def _insert_recursive(self, node: TreeNode[T], value: T) -> None:
        if value < node.value:
            if node.left:
                self._insert_recursive(node.left, value)
            else:
                node.left = TreeNode(value)
        else:
            if node.right:
                self._insert_recursive(node.right, value)
            else:
                node.right = TreeNode(value)

    def in_order_iterator(self) -> InOrderIterator[T]:
        """Create in-order iterator."""
        return InOrderIterator(self._root)

    def pre_order_iterator(self) -> PreOrderIterator[T]:
        """Create pre-order iterator."""
        return PreOrderIterator(self._root)

    def post_order_iterator(self) -> PostOrderIterator[T]:
        """Create post-order iterator."""
        return PostOrderIterator(self._root)

    def level_order_iterator(self) -> LevelOrderIterator[T]:
        """Create level-order iterator."""
        return LevelOrderIterator(self._root)

    # Default iteration (in-order)
    def __iter__(self) -> InOrderIterator[T]:
        return self.in_order_iterator()


# Client code demonstration
if __name__ == "__main__":
    print("=== Iterator Pattern Demo ===\n")

    # Create a binary search tree
    #        5
    #       / \
    #      3   7
    #     / \ / \
    #    1  4 6  9

    tree: BinaryTree[int] = BinaryTree()
    for value in [5, 3, 7, 1, 4, 6, 9]:
        tree.insert(value)

    print("Tree structure:")
    print("       5")
    print("      / \\")
    print("     3   7")
    print("    / \\ / \\")
    print("   1  4 6  9")
    print()

    print("1. In-order traversal (sorted):")
    iterator = tree.in_order_iterator()
    result = []
    while iterator.has_next():
        result.append(next(iterator))
    print(f"   {result}")

    print("\n2. Pre-order traversal (root first):")
    result = list(tree.pre_order_iterator())
    print(f"   {result}")

    print("\n3. Post-order traversal (root last):")
    result = list(tree.post_order_iterator())
    print(f"   {result}")

    print("\n4. Level-order traversal (BFS):")
    result = list(tree.level_order_iterator())
    print(f"   {result}")

    print("\n5. Using Python's for loop (default in-order):")
    print(f"   {[x for x in tree]}")
```

---

## 4. Mediator Pattern (中介者模式)

**Define an object that encapsulates how a set of objects interact, promoting loose coupling by keeping objects from referring to each other explicitly.**

### 中文详解

中介者模式是一种行为型设计模式，它让你减少对象之间混乱无序的依赖关系。该模式会限制对象之间的直接交互，迫使它们通过一个中介者对象进行合作。

**适用场景：**
- 当一些对象和其他对象紧密耦合以致难以对其进行修改时
- 当组件因过于依赖其他组件而无法在不同应用中复用时
- 当为了在不同场景下复用一些基本行为而不得不创建大量组件子类时
- 例如：GUI 对话框、聊天室、航空交通管制

**优点：**
- 单一职责原则：将多个组件间的交流抽取到单一位置
- 开闭原则：无需修改实际组件就能引入新的中介者
- 可以减轻应用中多个组件间的耦合情况
- 可以更方便地复用各个组件

**缺点：**
- 中介者可能会演化成为上帝对象

### Structure Diagram

```
+-------------------+            +-------------------+
|     Mediator      |            |    Colleague      |
|   <<interface>>   |            |   <<abstract>>    |
+-------------------+            +-------------------+
| + notify(sender,  |<-----------| - mediator        |
|         event)    |            +-------------------+
+-------------------+            | + set_mediator()  |
         ^                       +-------------------+
         |                                ^
         |                                |
+-------------------+         +-----------+-----------+
| ConcreteMediator  |         |                       |
+-------------------+    +-----------+          +-----------+
| - componentA      |--->| ColleagueA|          | ColleagueB|
| - componentB      |--->+-----------+          +-----------+
| - componentC      |    | + action()|          | + action()|
+-------------------+    +-----------+          +-----------+
| + notify(sender,  |
|         event)    |
+-------------------+

Communication Flow:
   ColleagueA ---notify---> Mediator ---action---> ColleagueB
                                    ---action---> ColleagueC
```

**图解说明：**
- `Mediator` 定义同事对象通信的接口
- `ConcreteMediator` 实现协调各同事对象的交互
- `Colleague` 每个同事类只知道中介者，不知道其他同事
- 同事对象通过中介者互相通信

### Python Code Example

```python
"""
Mediator Pattern Implementation in Python
中介者模式的 Python 实现

Example: Smart home system with interconnected devices
示例：具有互联设备的智能家居系统
"""

from abc import ABC, abstractmethod
from typing import Dict, List
from datetime import datetime


# Mediator interface
class SmartHomeMediator(ABC):
    """
    Mediator interface for smart home devices.
    智能家居设备的中介者接口。
    """
    @abstractmethod
    def notify(self, sender: 'SmartDevice', event: str, data: dict = None) -> None:
        pass


# Colleague base class
class SmartDevice(ABC):
    """
    Abstract colleague: Smart device base class.
    抽象同事：智能设备基类。
    """
    def __init__(self, name: str):
        self._name = name
        self._mediator: SmartHomeMediator = None
        self._state = "off"

    @property
    def name(self) -> str:
        return self._name

    @property
    def state(self) -> str:
        return self._state

    def set_mediator(self, mediator: SmartHomeMediator) -> None:
        self._mediator = mediator

    def log(self, message: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        print(f"  [{timestamp}] {self._name}: {message}")


# Concrete Colleagues
class MotionSensor(SmartDevice):
    """Motion sensor device."""
    def __init__(self, name: str, location: str):
        super().__init__(name)
        self._location = location

    def detect_motion(self) -> None:
        self.log(f"Motion detected in {self._location}!")
        self._mediator.notify(self, "motion_detected", 
                              {"location": self._location})


class SmartLight(SmartDevice):
    """Smart light device."""
    def __init__(self, name: str, room: str):
        super().__init__(name)
        self._room = room
        self._brightness = 0

    def turn_on(self, brightness: int = 100) -> None:
        self._state = "on"
        self._brightness = brightness
        self.log(f"Light ON at {brightness}% brightness")

    def turn_off(self) -> None:
        self._state = "off"
        self._brightness = 0
        self.log("Light OFF")


class SmartThermostat(SmartDevice):
    """Smart thermostat device."""
    def __init__(self, name: str):
        super().__init__(name)
        self._temperature = 20
        self._target = 22

    def set_temperature(self, target: int) -> None:
        self._target = target
        self.log(f"Target temperature set to {target}°C")
        self._mediator.notify(self, "temperature_changed", 
                              {"target": target})

    def get_temperature(self) -> int:
        return self._temperature


class SecurityAlarm(SmartDevice):
    """Security alarm device."""
    def __init__(self, name: str):
        super().__init__(name)
        self._armed = False

    def arm(self) -> None:
        self._armed = True
        self._state = "armed"
        self.log("Alarm ARMED")
        self._mediator.notify(self, "alarm_armed")

    def disarm(self) -> None:
        self._armed = False
        self._state = "disarmed"
        self.log("Alarm DISARMED")
        self._mediator.notify(self, "alarm_disarmed")

    def trigger(self) -> None:
        if self._armed:
            self.log("🚨 ALARM TRIGGERED!")
            self._mediator.notify(self, "alarm_triggered")


class SmartDoorLock(SmartDevice):
    """Smart door lock device."""
    def __init__(self, name: str):
        super().__init__(name)
        self._locked = True

    def lock(self) -> None:
        self._locked = True
        self._state = "locked"
        self.log("Door LOCKED")

    def unlock(self) -> None:
        self._locked = False
        self._state = "unlocked"
        self.log("Door UNLOCKED")
        self._mediator.notify(self, "door_unlocked")


# Concrete Mediator
class SmartHomeCentral(SmartHomeMediator):
    """
    Concrete mediator: Smart home central controller.
    具体中介者：智能家居中央控制器。
    """
    def __init__(self):
        self._devices: Dict[str, SmartDevice] = {}
        self._lights: List[SmartLight] = []
        self._alarm: SecurityAlarm = None
        self._thermostat: SmartThermostat = None

    def register_device(self, device: SmartDevice) -> None:
        """Register a device with the mediator."""
        device.set_mediator(self)
        self._devices[device.name] = device

        # Keep track of specific device types
        if isinstance(device, SmartLight):
            self._lights.append(device)
        elif isinstance(device, SecurityAlarm):
            self._alarm = device
        elif isinstance(device, SmartThermostat):
            self._thermostat = device

    def notify(self, sender: SmartDevice, event: str, data: dict = None) -> None:
        """
        Handle events from devices and coordinate responses.
        处理来自设备的事件并协调响应。
        """
        print(f"\n  [Central] Received '{event}' from {sender.name}")

        if event == "motion_detected":
            self._handle_motion(data.get("location", "unknown"))

        elif event == "alarm_armed":
            self._handle_alarm_armed()

        elif event == "alarm_disarmed":
            self._handle_alarm_disarmed()

        elif event == "alarm_triggered":
            self._handle_alarm_triggered()

        elif event == "door_unlocked":
            self._handle_door_unlocked()

        elif event == "temperature_changed":
            self._handle_temperature_change(data.get("target", 20))

    def _handle_motion(self, location: str) -> None:
        """Handle motion detection."""
        # Turn on lights in that area
        for light in self._lights:
            if location.lower() in light.name.lower():
                light.turn_on(50)

        # If alarm is armed, trigger it
        if self._alarm and self._alarm.state == "armed":
            self._alarm.trigger()

    def _handle_alarm_armed(self) -> None:
        """Handle alarm being armed."""
        # Turn off all lights
        for light in self._lights:
            light.turn_off()
        print("  [Central] All lights turned off for night mode")

    def _handle_alarm_disarmed(self) -> None:
        """Handle alarm being disarmed."""
        # Turn on entry lights
        for light in self._lights:
            if "entry" in light.name.lower():
                light.turn_on(100)

    def _handle_alarm_triggered(self) -> None:
        """Handle alarm being triggered."""
        # Turn on all lights to full brightness
        for light in self._lights:
            light.turn_on(100)
        print("  [Central] All lights turned ON (security mode)")

    def _handle_door_unlocked(self) -> None:
        """Handle door being unlocked."""
        if self._alarm and self._alarm.state == "armed":
            # Give 30 seconds to disarm (simulated)
            print("  [Central] Door unlocked while armed! Please disarm within 30s")

    def _handle_temperature_change(self, target: int) -> None:
        """Handle temperature changes."""
        if target < 18:
            print("  [Central] Low temperature target - activating heating mode")
        elif target > 25:
            print("  [Central] High temperature target - activating cooling mode")


# Client code demonstration
if __name__ == "__main__":
    print("=== Mediator Pattern Demo ===\n")

    # Create the mediator
    central = SmartHomeCentral()

    # Create devices
    living_room_sensor = MotionSensor("Living Room Sensor", "living room")
    living_room_light = SmartLight("Living Room Light", "living room")
    entry_light = SmartLight("Entry Light", "entry")
    bedroom_light = SmartLight("Bedroom Light", "bedroom")
    thermostat = SmartThermostat("Main Thermostat")
    alarm = SecurityAlarm("Home Alarm")
    front_door = SmartDoorLock("Front Door")

    # Register all devices
    for device in [living_room_sensor, living_room_light, entry_light,
                   bedroom_light, thermostat, alarm, front_door]:
        central.register_device(device)

    print("Scenario 1: Arriving home")
    print("-" * 40)
    front_door.unlock()
    print()

    print("Scenario 2: Motion detected (alarm disarmed)")
    print("-" * 40)
    living_room_sensor.detect_motion()
    print()

    print("Scenario 3: Going to bed (arm alarm)")
    print("-" * 40)
    alarm.arm()
    print()

    print("Scenario 4: Motion detected (alarm armed)")
    print("-" * 40)
    living_room_sensor.detect_motion()
    print()

    print("Scenario 5: Adjusting temperature")
    print("-" * 40)
    alarm.disarm()
    thermostat.set_temperature(28)
```

---

## 5. Memento Pattern (备忘录模式)

**Without violating encapsulation, capture and externalize an object's internal state so that the object can be restored to this state later.**

### 中文详解

备忘录模式是一种行为型设计模式，它允许在不暴露对象实现细节的情况下保存和恢复对象之前的状态。

**适用场景：**
- 当需要创建对象状态快照以便在需要时恢复对象时
- 当直接访问对象的成员变量、获取器或设置器将导致封装被破坏时
- 例如：撤销/重做功能、游戏存档、事务回滚

**优点：**
- 可以在不破坏对象封装情况的前提下创建对象状态快照
- 可以通过让负责人维护原发器状态历史记录来简化原发器代码

**缺点：**
- 如果客户端过于频繁地创建备忘录，可能会消耗大量内存
- 负责人必须完整跟踪原发器的生命周期，才能销毁弃用的备忘录

### Structure Diagram

```
+-------------------+        +-------------------+
|    Originator     |        |     Memento       |
+-------------------+        +-------------------+
| - state           |        | - state           |
+-------------------+        +-------------------+
| + save(): Memento |------->| + get_state()     |
| + restore(m)      |<-------| (only Originator  |
+-------------------+        |  can access)      |
                             +-------------------+
                                      ^
                                      |
                                      | stores
                                      |
                             +-------------------+
                             |    Caretaker      |
                             +-------------------+
                             | - mementos[]      |
                             +-------------------+
                             | + backup()        |
                             | + undo()          |
                             +-------------------+

Access Control:
- Originator: Full access to Memento's state
- Caretaker: Cannot access Memento's internal state
- This preserves encapsulation
```

**图解说明：**
- `Originator` 原发器，创建备忘录和从备忘录恢复状态
- `Memento` 备忘录，存储原发器的内部状态
- `Caretaker` 负责人，保存备忘录但不能操作其内容
- 只有原发器可以访问备忘录的状态

### Python Code Example

```python
"""
Memento Pattern Implementation in Python
备忘录模式的 Python 实现

Example: Game save/load system
示例：游戏存档/读档系统
"""

from __future__ import annotations
from abc import ABC, abstractmethod
from datetime import datetime
from typing import List, Dict, Any
import copy


# Memento
class GameMemento:
    """
    Memento: Stores game state snapshot.
    备忘录：存储游戏状态快照。
    """
    def __init__(self, state: Dict[str, Any], description: str = ""):
        self._state = copy.deepcopy(state)  # Deep copy to prevent mutation
        self._timestamp = datetime.now()
        self._description = description

    def get_state(self) -> Dict[str, Any]:
        """Only Originator should call this."""
        return copy.deepcopy(self._state)

    def get_timestamp(self) -> datetime:
        return self._timestamp

    def get_description(self) -> str:
        return self._description

    def __str__(self) -> str:
        time_str = self._timestamp.strftime("%H:%M:%S")
        return f"[{time_str}] {self._description}"


# Originator
class GameCharacter:
    """
    Originator: Game character whose state can be saved/restored.
    原发器：可以保存/恢复状态的游戏角色。
    """
    def __init__(self, name: str):
        self._name = name
        self._level = 1
        self._health = 100
        self._mana = 50
        self._position = {"x": 0, "y": 0}
        self._inventory: List[str] = []
        self._experience = 0

    def play(self, action: str) -> None:
        """Simulate gameplay actions."""
        if action == "fight":
            self._health -= 20
            self._mana -= 10
            self._experience += 30
            self._check_level_up()
            print(f"  {self._name} fought a monster! HP: {self._health}, XP: {self._experience}")

        elif action == "heal":
            self._health = min(100, self._health + 30)
            self._mana -= 15
            print(f"  {self._name} healed! HP: {self._health}")

        elif action == "move":
            self._position["x"] += 10
            self._position["y"] += 5
            print(f"  {self._name} moved to {self._position}")

        elif action == "pickup":
            item = "Sword of Power"
            self._inventory.append(item)
            print(f"  {self._name} picked up: {item}")

        elif action == "die":
            self._health = 0
            print(f"  💀 {self._name} has died!")

    def _check_level_up(self) -> None:
        new_level = 1 + self._experience // 100
        if new_level > self._level:
            self._level = new_level
            self._health = 100  # Full heal on level up
            self._mana = 50 + self._level * 10
            print(f"  🎉 Level up! {self._name} is now level {self._level}")

    def save(self, description: str = "") -> GameMemento:
        """Create a memento of current state."""
        state = {
            "name": self._name,
            "level": self._level,
            "health": self._health,
            "mana": self._mana,
            "position": self._position.copy(),
            "inventory": self._inventory.copy(),
            "experience": self._experience
        }
        return GameMemento(state, description or f"Lv{self._level} HP:{self._health}")

    def restore(self, memento: GameMemento) -> None:
        """Restore state from memento."""
        state = memento.get_state()
        self._name = state["name"]
        self._level = state["level"]
        self._health = state["health"]
        self._mana = state["mana"]
        self._position = state["position"]
        self._inventory = state["inventory"]
        self._experience = state["experience"]
        print(f"  ⏪ Restored to: {memento.get_description()}")

    def show_status(self) -> None:
        """Display current status."""
        print(f"  --- {self._name} Status ---")
        print(f"  Level: {self._level} | XP: {self._experience}")
        print(f"  HP: {self._health} | Mana: {self._mana}")
        print(f"  Position: {self._position}")
        print(f"  Inventory: {self._inventory if self._inventory else 'Empty'}")


# Caretaker
class SaveManager:
    """
    Caretaker: Manages save slots and history.
    负责人：管理存档槽位和历史。
    """
    def __init__(self, character: GameCharacter, max_autosaves: int = 5):
        self._character = character
        self._autosaves: List[GameMemento] = []
        self._manual_saves: Dict[str, GameMemento] = {}
        self._max_autosaves = max_autosaves

    def autosave(self, description: str = "") -> None:
        """Create an autosave."""
        memento = self._character.save(description)
        self._autosaves.append(memento)

        # Keep only recent autosaves
        if len(self._autosaves) > self._max_autosaves:
            self._autosaves.pop(0)

        print(f"  💾 Autosave created: {memento}")

    def manual_save(self, slot_name: str, description: str = "") -> None:
        """Create a manual save in a named slot."""
        memento = self._character.save(description)
        self._manual_saves[slot_name] = memento
        print(f"  💾 Saved to slot '{slot_name}': {memento}")

    def load_autosave(self, index: int = -1) -> bool:
        """Load an autosave by index (default: most recent)."""
        if not self._autosaves:
            print("  ❌ No autosaves available")
            return False

        memento = self._autosaves[index]
        self._character.restore(memento)
        return True

    def load_save(self, slot_name: str) -> bool:
        """Load a manual save from a named slot."""
        if slot_name not in self._manual_saves:
            print(f"  ❌ Save slot '{slot_name}' not found")
            return False

        memento = self._manual_saves[slot_name]
        self._character.restore(memento)
        return True

    def undo(self) -> bool:
        """Undo to the previous autosave."""
        if len(self._autosaves) < 2:
            print("  ❌ Nothing to undo")
            return False

        self._autosaves.pop()  # Remove current state
        return self.load_autosave(-1)

    def list_saves(self) -> None:
        """List all available saves."""
        print("\n  --- Available Saves ---")
        print("  Autosaves:")
        for i, save in enumerate(self._autosaves):
            print(f"    [{i}] {save}")

        print("  Manual Saves:")
        for slot, save in self._manual_saves.items():
            print(f"    [{slot}] {save}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Memento Pattern Demo ===\n")

    # Create game character and save manager
    hero = GameCharacter("Aldric")
    save_manager = SaveManager(hero)

    print("1. Starting the game:")
    hero.show_status()
    save_manager.autosave("Game Start")
    print()

    print("2. Playing the game:")
    hero.play("move")
    hero.play("fight")
    hero.play("pickup")
    save_manager.autosave("After first fight")
    print()

    hero.play("fight")
    hero.play("fight")
    hero.play("heal")
    save_manager.manual_save("checkpoint1", "Before boss fight")
    print()

    print("3. Boss fight goes wrong:")
    hero.play("fight")
    hero.play("fight")
    hero.play("die")
    hero.show_status()
    print()

    print("4. Load the manual save:")
    save_manager.load_save("checkpoint1")
    hero.show_status()
    print()

    print("5. List all saves:")
    save_manager.list_saves()
```

---

## 6. Observer Pattern (观察者模式)

**Define a one-to-many dependency between objects so that when one object changes state, all its dependents are notified and updated automatically.**

### 中文详解

观察者模式是一种行为型设计模式，它定义对象间的一种一对多的依赖关系，当一个对象的状态发生改变时，所有依赖于它的对象都会得到通知并被自动更新。

**适用场景：**
- 当一个对象状态的改变需要同时改变其他对象，而且不知道具体有多少对象需要改变时
- 当一个对象需要通知其他对象，但又不希望与被通知的对象形成紧耦合时
- 例如：事件处理系统、GUI 组件、股票行情、消息订阅

**优点：**
- 开闭原则：无需修改发布者代码就能引入新的订阅者类
- 可以在运行时建立对象之间的关系

**缺点：**
- 订阅者的通知顺序是随机的

### Structure Diagram

```
+-------------------+          +-------------------+
|     Subject       |          |     Observer      |
|   (Publisher)     |          |   <<interface>>   |
+-------------------+          +-------------------+
| - observers[]     |--------->| + update(data)    |
+-------------------+          +-------------------+
| + attach(observer)|                   ^
| + detach(observer)|                   |
| + notify()        |          +--------+--------+
+-------------------+          |                 |
         ^             +---------------+ +---------------+
         |             |ConcreteObsrvr | |ConcreteObsrvr |
+-------------------+  |      A        | |      B        |
| ConcreteSubject   |  +---------------+ +---------------+
+-------------------+  | + update()    | | + update()    |
| - state           |  +---------------+ +---------------+
+-------------------+
| + get_state()     |
| + set_state()     |
+-------------------+

Notification Flow:
  Subject.set_state() --> notify() --> Observer.update()
                                   --> Observer.update()
                                   --> Observer.update()
```

**图解说明：**
- `Subject` 主题/发布者，维护观察者列表，状态变化时通知观察者
- `Observer` 观察者接口，定义更新接口
- `ConcreteSubject` 具体主题，存储状态，状态变化时通知观察者
- `ConcreteObserver` 具体观察者，实现更新接口

### Python Code Example

```python
"""
Observer Pattern Implementation in Python
观察者模式的 Python 实现

Example: Stock market price updates
示例：股票市场价格更新
"""

from abc import ABC, abstractmethod
from typing import List, Dict, Any
from datetime import datetime
import random


# Observer interface
class Observer(ABC):
    """
    Observer interface.
    观察者接口。
    """
    @abstractmethod
    def update(self, subject: 'Subject') -> None:
        pass


# Subject interface
class Subject(ABC):
    """
    Subject (Publisher) interface.
    主题（发布者）接口。
    """
    @abstractmethod
    def attach(self, observer: Observer) -> None:
        pass

    @abstractmethod
    def detach(self, observer: Observer) -> None:
        pass

    @abstractmethod
    def notify(self) -> None:
        pass


# Concrete Subject
class StockMarket(Subject):
    """
    Concrete subject: Stock market with price updates.
    具体主题：带有价格更新的股票市场。
    """
    def __init__(self):
        self._observers: List[Observer] = []
        self._stocks: Dict[str, float] = {}
        self._last_updated: Dict[str, datetime] = {}

    def attach(self, observer: Observer) -> None:
        if observer not in self._observers:
            self._observers.append(observer)
            print(f"  StockMarket: {observer.__class__.__name__} subscribed")

    def detach(self, observer: Observer) -> None:
        if observer in self._observers:
            self._observers.remove(observer)
            print(f"  StockMarket: {observer.__class__.__name__} unsubscribed")

    def notify(self) -> None:
        print(f"  StockMarket: Notifying {len(self._observers)} observers...")
        for observer in self._observers:
            observer.update(self)

    def set_stock_price(self, symbol: str, price: float) -> None:
        """Update stock price and notify observers."""
        old_price = self._stocks.get(symbol, price)
        self._stocks[symbol] = price
        self._last_updated[symbol] = datetime.now()

        change = ((price - old_price) / old_price * 100) if old_price else 0
        direction = "📈" if change > 0 else "📉" if change < 0 else "➡️"

        print(f"\n{direction} {symbol}: ${old_price:.2f} -> ${price:.2f} ({change:+.2f}%)")
        self.notify()

    def get_stock_price(self, symbol: str) -> float:
        return self._stocks.get(symbol, 0.0)

    def get_all_stocks(self) -> Dict[str, float]:
        return self._stocks.copy()


# Concrete Observers
class StockDisplay(Observer):
    """
    Concrete observer: Display stock prices.
    具体观察者：显示股票价格。
    """
    def __init__(self, name: str):
        self._name = name

    def update(self, subject: StockMarket) -> None:
        stocks = subject.get_all_stocks()
        print(f"    [{self._name}] Current prices: ", end="")
        print(", ".join(f"{s}: ${p:.2f}" for s, p in stocks.items()))


class StockAlert(Observer):
    """
    Concrete observer: Alert on significant price changes.
    具体观察者：在价格显著变化时发出警报。
    """
    def __init__(self, symbol: str, threshold: float):
        self._symbol = symbol
        self._threshold = threshold
        self._last_price = 0.0

    def update(self, subject: StockMarket) -> None:
        current_price = subject.get_stock_price(self._symbol)
        
        if self._last_price > 0:
            change = abs((current_price - self._last_price) / self._last_price * 100)
            if change >= self._threshold:
                print(f"    🚨 ALERT: {self._symbol} moved {change:.2f}% "
                      f"(threshold: {self._threshold}%)")

        self._last_price = current_price


class StockLogger(Observer):
    """
    Concrete observer: Log all price changes.
    具体观察者：记录所有价格变化。
    """
    def __init__(self):
        self._log: List[str] = []

    def update(self, subject: StockMarket) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        stocks = subject.get_all_stocks()
        
        entry = f"[{timestamp}] " + ", ".join(
            f"{s}=${p:.2f}" for s, p in stocks.items()
        )
        self._log.append(entry)
        print(f"    [Logger] Recorded: {entry}")

    def get_log(self) -> List[str]:
        return self._log.copy()


class PortfolioTracker(Observer):
    """
    Concrete observer: Track portfolio value.
    具体观察者：跟踪投资组合价值。
    """
    def __init__(self, holdings: Dict[str, int]):
        self._holdings = holdings  # symbol -> quantity

    def update(self, subject: StockMarket) -> None:
        total_value = 0.0
        for symbol, quantity in self._holdings.items():
            price = subject.get_stock_price(symbol)
            total_value += price * quantity

        print(f"    [Portfolio] Total value: ${total_value:,.2f}")


# Client code demonstration
if __name__ == "__main__":
    print("=== Observer Pattern Demo ===\n")

    # Create the subject
    market = StockMarket()

    # Create observers
    display = StockDisplay("Main Display")
    aapl_alert = StockAlert("AAPL", threshold=5.0)
    logger = StockLogger()
    portfolio = PortfolioTracker({"AAPL": 10, "GOOGL": 5, "MSFT": 20})

    # Attach observers
    print("Subscribing observers:")
    market.attach(display)
    market.attach(aapl_alert)
    market.attach(logger)
    market.attach(portfolio)

    # Simulate price updates
    print("\n" + "=" * 50)
    print("Simulating market updates...")
    print("=" * 50)

    # Initial prices
    market.set_stock_price("AAPL", 150.00)
    market.set_stock_price("GOOGL", 2800.00)
    market.set_stock_price("MSFT", 310.00)

    # Price changes
    market.set_stock_price("AAPL", 155.00)  # Small change
    market.set_stock_price("AAPL", 165.00)  # Bigger change - should trigger alert
    market.set_stock_price("GOOGL", 2850.00)

    # Detach an observer
    print("\n" + "=" * 50)
    print("Unsubscribing display...")
    market.detach(display)
    market.set_stock_price("MSFT", 320.00)

    # Show log
    print("\n" + "=" * 50)
    print("Complete price log:")
    for entry in logger.get_log():
        print(f"  {entry}")
```

---

## Summary

| Pattern | Purpose | Key Benefit |
|---------|---------|-------------|
| **Chain of Responsibility** | Pass request along a chain | Decouples sender from receiver |
| **Command** | Encapsulate request as object | Supports undo/redo |
| **Iterator** | Sequential access without exposing internals | Uniform traversal interface |
| **Mediator** | Centralize complex communications | Reduces coupling between objects |
| **Memento** | Capture and restore object state | Preserves encapsulation |
| **Observer** | One-to-many dependency notification | Loose coupling |

---

*Continue: See `design_patterns_behavioral_2.md` for more behavioral patterns.*

