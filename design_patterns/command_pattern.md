# Command Pattern (命令模式)

## ASCII Diagram

```
+-------------------+     +-------------------+
|      Client       |     |     Invoker       |
+-------------------+     +-------------------+
| + setup()         |---->| - command         |
+-------------------+     +-------------------+
         |                | + setCommand()    |
         |                | + executeCommand()|
         |                +---------+---------+
         |                          |
         |                          | calls
         v                          v
+-------------------+     +-------------------+
|     Receiver      |<----+     Command       |<<interface>>
+-------------------+     +-------------------+
| + action()        |     | + execute()       |
+-------------------+     | + undo()          |
         ^                +-------------------+
         |                          ^
         |                          |
         |           +--------------+--------------+
         |           |                             |
         |  +--------+--------+          +--------+--------+
         +--|ConcreteCommandA |          |ConcreteCommandB |
            +-----------------+          +-----------------+
            | - receiver      |          | - receiver      |
            | - params        |          | - params        |
            +-----------------+          +-----------------+
            | + execute()     |          | + execute()     |
            | + undo()        |          | + undo()        |
            +-----------------+          +-----------------+

Command History (for undo/redo):
+--------+--------+--------+--------+
| Cmd 1  | Cmd 2  | Cmd 3  | Cmd 4  |  <- History Stack
+--------+--------+--------+--------+
                     ^
                     |
                  Current
```

**中文说明：**
- **Command（命令接口）**：声明执行操作的接口
- **ConcreteCommand（具体命令）**：绑定接收者与动作
- **Receiver（接收者）**：知道如何执行相关操作
- **Invoker（调用者）**：要求命令执行请求
- **Client（客户端）**：创建具体命令并设置接收者

---

## 核心思想

将一个**请求封装为一个对象**，从而使你可以用不同的请求对客户进行参数化，对请求排队或记录请求日志，以及支持可撤销的操作。命令模式将发起请求的对象与执行请求的对象解耦。

---

## 应用场景

1. **参数化对象**：需要在不同时间指定、排列和执行请求
2. **撤销/重做**：需要支持撤销和重做功能
3. **事务系统**：需要支持事务（一组操作要么全部执行，要么全部不执行）
4. **实际应用**：
   - 文本编辑器的撤销/重做
   - GUI 按钮和菜单操作
   - 任务队列
   - 宏录制
   - 数据库事务

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 解耦 | 发送者与接收者解耦 |
| 扩展性 | 易于添加新命令 |
| 可组合 | 可以组合多个命令 |
| 撤销/重做 | 容易实现撤销和重做 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 类数量增加 | 每个命令都需要一个类 |
| 复杂度 | 简单操作可能过度设计 |

---

## Python 代码示例

### 应用前：直接调用方法

```python
# 问题：文本编辑器，操作直接调用，无法撤销

class TextEditor:
    def __init__(self):
        self.content = ""
    
    def write(self, text):
        self.content += text
        print(f"Written: '{text}'")
    
    def delete(self, count):
        deleted = self.content[-count:]
        self.content = self.content[:-count]
        print(f"Deleted: '{deleted}'")
    
    def get_content(self):
        return self.content


# 使用
editor = TextEditor()
editor.write("Hello ")
editor.write("World")
editor.delete(3)
print(f"Content: {editor.get_content()}")

# 问题：
# 1. 无法撤销操作
# 2. 无法记录操作历史
# 3. 无法批量执行操作
# 4. 无法延迟执行
```

### 应用后：使用命令模式

```python
from abc import ABC, abstractmethod
from typing import List, Optional, Any
from dataclasses import dataclass
from datetime import datetime
import copy


# ========== 命令接口 ==========
class Command(ABC):
    """命令抽象基类"""
    
    @abstractmethod
    def execute(self) -> bool:
        """执行命令"""
        pass
    
    @abstractmethod
    def undo(self) -> bool:
        """撤销命令"""
        pass
    
    @property
    @abstractmethod
    def description(self) -> str:
        """命令描述"""
        pass


# ========== 接收者 ==========
class TextDocument:
    """文本文档 - 接收者"""
    
    def __init__(self, content: str = ""):
        self._content = content
        self._clipboard = ""
    
    @property
    def content(self) -> str:
        return self._content
    
    @content.setter
    def content(self, value: str):
        self._content = value
    
    def insert(self, position: int, text: str):
        """在指定位置插入文本"""
        self._content = self._content[:position] + text + self._content[position:]
    
    def delete(self, start: int, end: int) -> str:
        """删除指定范围的文本，返回被删除的内容"""
        deleted = self._content[start:end]
        self._content = self._content[:start] + self._content[end:]
        return deleted
    
    def replace(self, start: int, end: int, text: str) -> str:
        """替换指定范围的文本"""
        old_text = self._content[start:end]
        self._content = self._content[:start] + text + self._content[end:]
        return old_text
    
    def copy_to_clipboard(self, start: int, end: int):
        """复制到剪贴板"""
        self._clipboard = self._content[start:end]
    
    def paste_from_clipboard(self, position: int) -> str:
        """从剪贴板粘贴"""
        self.insert(position, self._clipboard)
        return self._clipboard
    
    @property
    def clipboard(self) -> str:
        return self._clipboard


# ========== 具体命令 ==========
class InsertCommand(Command):
    """插入命令"""
    
    def __init__(self, document: TextDocument, position: int, text: str):
        self._document = document
        self._position = position
        self._text = text
    
    def execute(self) -> bool:
        self._document.insert(self._position, self._text)
        return True
    
    def undo(self) -> bool:
        self._document.delete(self._position, self._position + len(self._text))
        return True
    
    @property
    def description(self) -> str:
        preview = self._text[:20] + "..." if len(self._text) > 20 else self._text
        return f"Insert '{preview}' at {self._position}"


class DeleteCommand(Command):
    """删除命令"""
    
    def __init__(self, document: TextDocument, start: int, end: int):
        self._document = document
        self._start = start
        self._end = end
        self._deleted_text = ""  # 保存被删除的文本用于撤销
    
    def execute(self) -> bool:
        self._deleted_text = self._document.delete(self._start, self._end)
        return True
    
    def undo(self) -> bool:
        self._document.insert(self._start, self._deleted_text)
        return True
    
    @property
    def description(self) -> str:
        return f"Delete from {self._start} to {self._end}"


class ReplaceCommand(Command):
    """替换命令"""
    
    def __init__(self, document: TextDocument, start: int, end: int, new_text: str):
        self._document = document
        self._start = start
        self._end = end
        self._new_text = new_text
        self._old_text = ""
    
    def execute(self) -> bool:
        self._old_text = self._document.replace(self._start, self._end, self._new_text)
        return True
    
    def undo(self) -> bool:
        self._document.replace(self._start, self._start + len(self._new_text), self._old_text)
        return True
    
    @property
    def description(self) -> str:
        return f"Replace '{self._old_text}' with '{self._new_text}'"


class CopyCommand(Command):
    """复制命令（不可撤销）"""
    
    def __init__(self, document: TextDocument, start: int, end: int):
        self._document = document
        self._start = start
        self._end = end
    
    def execute(self) -> bool:
        self._document.copy_to_clipboard(self._start, self._end)
        return True
    
    def undo(self) -> bool:
        # 复制操作不需要撤销
        return True
    
    @property
    def description(self) -> str:
        return f"Copy from {self._start} to {self._end}"


class PasteCommand(Command):
    """粘贴命令"""
    
    def __init__(self, document: TextDocument, position: int):
        self._document = document
        self._position = position
        self._pasted_text = ""
    
    def execute(self) -> bool:
        self._pasted_text = self._document.paste_from_clipboard(self._position)
        return True
    
    def undo(self) -> bool:
        self._document.delete(self._position, self._position + len(self._pasted_text))
        return True
    
    @property
    def description(self) -> str:
        return f"Paste at {self._position}"


class MacroCommand(Command):
    """宏命令 - 组合多个命令"""
    
    def __init__(self, name: str = "Macro"):
        self._name = name
        self._commands: List[Command] = []
    
    def add(self, command: Command):
        self._commands.append(command)
    
    def execute(self) -> bool:
        for cmd in self._commands:
            if not cmd.execute():
                return False
        return True
    
    def undo(self) -> bool:
        # 逆序撤销
        for cmd in reversed(self._commands):
            if not cmd.undo():
                return False
        return True
    
    @property
    def description(self) -> str:
        return f"{self._name} ({len(self._commands)} commands)"


# ========== 调用者 ==========
class CommandHistory:
    """命令历史管理器"""
    
    def __init__(self, max_history: int = 100):
        self._history: List[Command] = []
        self._redo_stack: List[Command] = []
        self._max_history = max_history
    
    def push(self, command: Command):
        """记录已执行的命令"""
        self._history.append(command)
        self._redo_stack.clear()  # 新操作清空重做栈
        
        # 限制历史长度
        if len(self._history) > self._max_history:
            self._history.pop(0)
    
    def undo(self) -> Optional[Command]:
        """撤销最后一个命令"""
        if not self._history:
            return None
        command = self._history.pop()
        self._redo_stack.append(command)
        return command
    
    def redo(self) -> Optional[Command]:
        """重做最后撤销的命令"""
        if not self._redo_stack:
            return None
        command = self._redo_stack.pop()
        self._history.append(command)
        return command
    
    def can_undo(self) -> bool:
        return len(self._history) > 0
    
    def can_redo(self) -> bool:
        return len(self._redo_stack) > 0
    
    def get_history(self) -> List[str]:
        return [cmd.description for cmd in self._history]


class TextEditor:
    """文本编辑器 - 调用者"""
    
    def __init__(self):
        self._document = TextDocument()
        self._history = CommandHistory()
    
    def execute(self, command: Command) -> bool:
        """执行命令"""
        result = command.execute()
        if result:
            self._history.push(command)
            print(f"✓ Executed: {command.description}")
        return result
    
    def undo(self) -> bool:
        """撤销"""
        command = self._history.undo()
        if command:
            command.undo()
            print(f"↶ Undone: {command.description}")
            return True
        print("✗ Nothing to undo")
        return False
    
    def redo(self) -> bool:
        """重做"""
        command = self._history.redo()
        if command:
            command.execute()
            print(f"↷ Redone: {command.description}")
            return True
        print("✗ Nothing to redo")
        return False
    
    # 便捷方法
    def insert(self, position: int, text: str):
        return self.execute(InsertCommand(self._document, position, text))
    
    def delete(self, start: int, end: int):
        return self.execute(DeleteCommand(self._document, start, end))
    
    def replace(self, start: int, end: int, new_text: str):
        return self.execute(ReplaceCommand(self._document, start, end, new_text))
    
    def copy(self, start: int, end: int):
        return self.execute(CopyCommand(self._document, start, end))
    
    def paste(self, position: int):
        return self.execute(PasteCommand(self._document, position))
    
    def write(self, text: str):
        """在末尾写入（常用操作）"""
        return self.insert(len(self._document.content), text)
    
    @property
    def content(self) -> str:
        return self._document.content
    
    @property
    def history(self) -> List[str]:
        return self._history.get_history()
    
    def print_status(self):
        print(f"\nContent: '{self.content}'")
        print(f"Can undo: {self._history.can_undo()}, Can redo: {self._history.can_redo()}")


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("Text Editor with Command Pattern")
    print("=" * 60)
    
    editor = TextEditor()
    
    # 基本操作
    print("\n--- Basic Operations ---")
    editor.write("Hello ")
    editor.write("World")
    editor.write("!")
    editor.print_status()
    
    # 撤销
    print("\n--- Undo Operations ---")
    editor.undo()
    editor.print_status()
    
    editor.undo()
    editor.print_status()
    
    # 重做
    print("\n--- Redo Operations ---")
    editor.redo()
    editor.print_status()
    
    # 插入和删除
    print("\n--- Insert and Delete ---")
    editor.insert(6, "Beautiful ")
    editor.print_status()
    
    editor.delete(6, 16)  # 删除 "Beautiful "
    editor.print_status()
    
    # 撤销删除
    editor.undo()
    editor.print_status()
    
    # 复制粘贴
    print("\n--- Copy and Paste ---")
    editor.copy(0, 5)  # 复制 "Hello"
    editor.paste(len(editor.content))  # 粘贴到末尾
    editor.print_status()
    
    # 替换
    print("\n--- Replace ---")
    editor.replace(0, 5, "Hi")
    editor.print_status()
    
    # 查看历史
    print("\n--- Command History ---")
    for i, desc in enumerate(editor.history, 1):
        print(f"  {i}. {desc}")
    
    # 宏命令
    print("\n--- Macro Command ---")
    editor2 = TextEditor()
    
    # 创建宏：添加签名
    signature_macro = MacroCommand("Add Signature")
    signature_macro.add(InsertCommand(editor2._document, 0, "---\n"))
    signature_macro.add(InsertCommand(editor2._document, 4, "Best regards,\n"))
    signature_macro.add(InsertCommand(editor2._document, 18, "John Doe\n"))
    signature_macro.add(InsertCommand(editor2._document, 27, "---"))
    
    editor2.execute(signature_macro)
    editor2.print_status()
    
    # 撤销整个宏
    print("\nUndo macro:")
    editor2.undo()
    editor2.print_status()


# ========== 任务队列示例 ==========
print("\n" + "=" * 60)
print("Task Queue with Command Pattern")
print("=" * 60)


@dataclass
class Task:
    """任务数据"""
    id: str
    data: Any


class TaskCommand(Command):
    """任务命令基类"""
    pass


class ProcessDataCommand(TaskCommand):
    """处理数据命令"""
    
    def __init__(self, task: Task):
        self._task = task
        self._result = None
    
    def execute(self) -> bool:
        print(f"  Processing task {self._task.id}: {self._task.data}")
        self._result = f"Processed: {self._task.data}"
        return True
    
    def undo(self) -> bool:
        print(f"  Reverting task {self._task.id}")
        self._result = None
        return True
    
    @property
    def description(self) -> str:
        return f"Process task {self._task.id}"


class SendEmailCommand(TaskCommand):
    """发送邮件命令"""
    
    def __init__(self, to: str, subject: str, body: str):
        self._to = to
        self._subject = subject
        self._body = body
        self._sent = False
    
    def execute(self) -> bool:
        print(f"  Sending email to {self._to}: {self._subject}")
        self._sent = True
        return True
    
    def undo(self) -> bool:
        if self._sent:
            print(f"  Cannot unsend email to {self._to}")
        return False  # 邮件无法撤销
    
    @property
    def description(self) -> str:
        return f"Send email to {self._to}"


class TaskQueue:
    """任务队列"""
    
    def __init__(self):
        self._queue: List[TaskCommand] = []
        self._executed: List[TaskCommand] = []
    
    def add(self, command: TaskCommand):
        self._queue.append(command)
        print(f"Queued: {command.description}")
    
    def process_all(self):
        print("\nProcessing queue...")
        while self._queue:
            cmd = self._queue.pop(0)
            if cmd.execute():
                self._executed.append(cmd)
    
    def process_one(self) -> bool:
        if not self._queue:
            return False
        cmd = self._queue.pop(0)
        if cmd.execute():
            self._executed.append(cmd)
        return True


# 使用任务队列
queue = TaskQueue()
queue.add(ProcessDataCommand(Task("T1", {"name": "Alice"})))
queue.add(ProcessDataCommand(Task("T2", {"name": "Bob"})))
queue.add(SendEmailCommand("admin@example.com", "Report", "Daily report..."))

queue.process_all()
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **撤销/重做** | 无法实现 | 每个命令可撤销 |
| **操作记录** | 无历史记录 | 完整的操作历史 |
| **延迟执行** | 立即执行 | 可以排队延迟执行 |
| **组合操作** | 难以组合 | 宏命令组合多个操作 |
| **解耦** | 调用者直接依赖接收者 | 通过命令对象解耦 |

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **Memento** | 常与命令模式一起使用，保存撤销所需的状态 |
| **Strategy** | 两者都封装算法，但命令更关注请求的执行和撤销 |
| **Chain of Responsibility** | 可以用命令作为请求对象传递 |

