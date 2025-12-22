# Command Pattern in Python

---

## 1. Pattern Name: Command (Action, Transaction)

**Purpose / Problem Solved:**
Encapsulate a request as an object, thereby letting you parameterize clients with different requests, queue or log requests, and support undoable operations. Decouples the invoker from the receiver.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                          INVOKER                                  |
|------------------------------------------------------------------|
| - command: Command                                                |
|------------------------------------------------------------------|
| + set_command(command: Command)                                   |
| + execute_command()                                               |
|     command.execute()                                             |
+------------------------------------------------------------------+
                              |
                              | Calls execute()
                              v
+------------------------------------------------------------------+
|                      COMMAND (Interface)                          |
|------------------------------------------------------------------|
| + execute()                                                       |
| + undo()         # optional                                       |
+------------------------------------------------------------------+
         ^                    ^                    ^
         |                    |                    |
+----------------+    +----------------+    +----------------+
| ConcreteCmd A  |    | ConcreteCmd B  |    | ConcreteCmd C  |
|----------------|    |----------------|    |----------------|
| - receiver: R  |    | - receiver: R  |    | - receiver: R  |
| - params       |    | - params       |    | - params       |
|----------------|    |----------------|    |----------------|
| execute():     |    | execute():     |    | execute():     |
|  receiver.act()|    |  receiver.act()|    |  receiver.act()|
| undo():        |    | undo():        |    | undo():        |
|  receiver.rev()|    |  receiver.rev()|    |  receiver.rev()|
+----------------+    +----------------+    +----------------+
         |                    |                    |
         v                    v                    v
+------------------------------------------------------------------+
|                         RECEIVER                                  |
|------------------------------------------------------------------|
| + action()           # Knows how to perform the operation        |
| + reverse_action()   # Knows how to undo                         |
+------------------------------------------------------------------+

COMMAND QUEUE/HISTORY:
+--------+     +--------+     +--------+     +--------+
| Cmd 1  | --> | Cmd 2  | --> | Cmd 3  | --> | Cmd 4  |
+--------+     +--------+     +--------+     +--------+
    |                                            ^
    v                                            |
[Execute in order]                        [Undo from here]
```

**中文说明：**
命令模式将请求封装为对象，使得可以用不同的请求参数化客户端、将请求排队或记录日志、支持撤销操作。Invoker不知道Receiver的存在，只知道Command接口。Command对象持有Receiver引用和所需参数。这实现了请求发送者和接收者的解耦。典型应用：GUI操作（菜单项、按钮）、事务系统、任务队列、宏录制、撤销/重做功能。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Command |
|----------------|-------------------------|
| **First-class functions** | Functions can BE commands (no class needed) |
| **`__call__`** | Make command objects callable |
| **`functools.partial`** | Create configured command functions |
| **Closures** | Capture state for undo operations |
| **Dataclasses** | Simple command class definitions |
| **Deque** | Efficient command history (undo/redo stack) |
| **`*args, **kwargs`** | Flexible command parameters |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **Django admin** | Admin actions as command objects |
| **Celery** | Task queue with command-like tasks |
| **Click** | CLI commands with @click.command decorator |
| **pytest** | Test functions as command objects |
| **Tkinter** | Button command parameter |
| **Text editors** | Undo/redo with command history |

```python
# Click CLI example
import click

@click.command()
@click.option('--name', default='World')
def hello(name):
    click.echo(f'Hello {name}!')

# Each CLI invocation is a "command" execution
```

---

## 5. Python Module Examples

### Example 1: Classic Command with Undo

```python
#!/usr/bin/env python3
"""
Command Pattern - Classic Implementation with Undo

Text editor example with undoable commands.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List
from collections import deque


# ============== RECEIVER ==============

class TextDocument:
    """
    Receiver: The object that knows how to perform operations.
    """
    
    def __init__(self):
        self.content: List[str] = []
        self.cursor: int = 0
    
    def insert(self, position: int, text: str):
        """Insert text at position."""
        if 0 <= position <= len(self.content):
            self.content.insert(position, text)
            self.cursor = position + 1
    
    def delete(self, position: int) -> str:
        """Delete text at position, return deleted text."""
        if 0 <= position < len(self.content):
            deleted = self.content.pop(position)
            self.cursor = min(position, len(self.content))
            return deleted
        return ""
    
    def replace(self, position: int, text: str) -> str:
        """Replace text at position, return old text."""
        if 0 <= position < len(self.content):
            old = self.content[position]
            self.content[position] = text
            return old
        return ""
    
    def get_text(self) -> str:
        """Get full document text."""
        return "".join(self.content)
    
    def __str__(self):
        return f"Document: '{self.get_text()}' (cursor: {self.cursor})"


# ============== COMMAND INTERFACE ==============

class Command(ABC):
    """Abstract command interface."""
    
    @abstractmethod
    def execute(self) -> None:
        """Execute the command."""
        pass
    
    @abstractmethod
    def undo(self) -> None:
        """Undo the command."""
        pass
    
    @property
    @abstractmethod
    def description(self) -> str:
        """Human-readable description."""
        pass


# ============== CONCRETE COMMANDS ==============

@dataclass
class InsertCommand(Command):
    """Command to insert text."""
    document: TextDocument
    position: int
    text: str
    
    def execute(self) -> None:
        self.document.insert(self.position, self.text)
    
    def undo(self) -> None:
        self.document.delete(self.position)
    
    @property
    def description(self) -> str:
        return f"Insert '{self.text}' at {self.position}"


@dataclass
class DeleteCommand(Command):
    """Command to delete text."""
    document: TextDocument
    position: int
    _deleted: str = field(default="", init=False)
    
    def execute(self) -> None:
        self._deleted = self.document.delete(self.position)
    
    def undo(self) -> None:
        self.document.insert(self.position, self._deleted)
    
    @property
    def description(self) -> str:
        return f"Delete at {self.position}"


@dataclass
class ReplaceCommand(Command):
    """Command to replace text."""
    document: TextDocument
    position: int
    new_text: str
    _old_text: str = field(default="", init=False)
    
    def execute(self) -> None:
        self._old_text = self.document.replace(self.position, self.new_text)
    
    def undo(self) -> None:
        self.document.replace(self.position, self._old_text)
    
    @property
    def description(self) -> str:
        return f"Replace at {self.position} with '{self.new_text}'"


# ============== MACRO COMMAND ==============

class MacroCommand(Command):
    """Command that executes multiple commands."""
    
    def __init__(self, name: str = "Macro"):
        self.name = name
        self.commands: List[Command] = []
    
    def add(self, command: Command):
        """Add command to macro."""
        self.commands.append(command)
    
    def execute(self) -> None:
        for cmd in self.commands:
            cmd.execute()
    
    def undo(self) -> None:
        # Undo in reverse order
        for cmd in reversed(self.commands):
            cmd.undo()
    
    @property
    def description(self) -> str:
        return f"Macro '{self.name}' ({len(self.commands)} commands)"


# ============== INVOKER ==============

class TextEditor:
    """
    Invoker: Executes commands and manages history.
    """
    
    def __init__(self, document: TextDocument):
        self.document = document
        self._history: deque = deque(maxlen=100)
        self._redo_stack: deque = deque()
    
    def execute(self, command: Command):
        """Execute command and add to history."""
        command.execute()
        self._history.append(command)
        self._redo_stack.clear()  # Clear redo on new action
        print(f"✓ Executed: {command.description}")
    
    def undo(self) -> bool:
        """Undo last command."""
        if self._history:
            command = self._history.pop()
            command.undo()
            self._redo_stack.append(command)
            print(f"↩ Undone: {command.description}")
            return True
        print("Nothing to undo")
        return False
    
    def redo(self) -> bool:
        """Redo last undone command."""
        if self._redo_stack:
            command = self._redo_stack.pop()
            command.execute()
            self._history.append(command)
            print(f"↪ Redone: {command.description}")
            return True
        print("Nothing to redo")
        return False
    
    def show_history(self):
        """Show command history."""
        print("\n📜 Command History:")
        for i, cmd in enumerate(self._history, 1):
            print(f"  {i}. {cmd.description}")
        if not self._history:
            print("  (empty)")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Command Pattern - Text Editor with Undo/Redo")
    print("=" * 60)
    
    # Create document and editor
    doc = TextDocument()
    editor = TextEditor(doc)
    
    print("\n--- Executing Commands ---")
    
    # Insert "Hello"
    editor.execute(InsertCommand(doc, 0, "H"))
    editor.execute(InsertCommand(doc, 1, "e"))
    editor.execute(InsertCommand(doc, 2, "l"))
    editor.execute(InsertCommand(doc, 3, "l"))
    editor.execute(InsertCommand(doc, 4, "o"))
    print(f"Current: {doc}")
    
    # Insert " World"
    editor.execute(InsertCommand(doc, 5, " "))
    editor.execute(InsertCommand(doc, 6, "W"))
    editor.execute(InsertCommand(doc, 7, "o"))
    editor.execute(InsertCommand(doc, 8, "r"))
    editor.execute(InsertCommand(doc, 9, "l"))
    editor.execute(InsertCommand(doc, 10, "d"))
    print(f"Current: {doc}")
    
    # Show history
    editor.show_history()
    
    # Undo some actions
    print("\n--- Undo Operations ---")
    editor.undo()
    editor.undo()
    editor.undo()
    print(f"Current: {doc}")
    
    # Redo
    print("\n--- Redo Operations ---")
    editor.redo()
    editor.redo()
    print(f"Current: {doc}")
    
    # Replace operation
    print("\n--- Replace Operation ---")
    editor.execute(ReplaceCommand(doc, 6, "P"))
    print(f"Current: {doc}")  # "Hello Porld"
    
    # Undo replace
    editor.undo()
    print(f"After undo: {doc}")  # "Hello World"
    
    # Macro command
    print("\n--- Macro Command ---")
    macro = MacroCommand("Add Exclamation")
    macro.add(InsertCommand(doc, 11, "!"))
    macro.add(InsertCommand(doc, 12, "!"))
    macro.add(InsertCommand(doc, 13, "!"))
    
    editor.execute(macro)
    print(f"Current: {doc}")  # "Hello World!!!"
    
    # Undo entire macro
    editor.undo()
    print(f"After undo macro: {doc}")  # "Hello World"
```

---

### Example 2: Function-Based Commands (Pythonic)

```python
#!/usr/bin/env python3
"""
Command Pattern - Pythonic with Functions

In Python, functions can BE commands.
Uses closures to capture undo state.
"""

from typing import Callable, Tuple, Optional
from collections import deque
from dataclasses import dataclass
from functools import partial


# ============== COMMAND TYPE ==============

# Command is a tuple of (execute_fn, undo_fn, description)
Command = Tuple[Callable[[], None], Callable[[], None], str]


def make_command(
    execute: Callable[[], None],
    undo: Callable[[], None],
    description: str = ""
) -> Command:
    """Create a command tuple."""
    return (execute, undo, description)


# ============== RECEIVER ==============

class BankAccount:
    """Bank account that supports transactions."""
    
    def __init__(self, owner: str, balance: float = 0):
        self.owner = owner
        self.balance = balance
    
    def deposit(self, amount: float):
        self.balance += amount
        print(f"  💰 Deposited ${amount:.2f}")
    
    def withdraw(self, amount: float) -> bool:
        if amount <= self.balance:
            self.balance -= amount
            print(f"  💸 Withdrew ${amount:.2f}")
            return True
        print(f"  ❌ Insufficient funds")
        return False
    
    def __str__(self):
        return f"Account({self.owner}): ${self.balance:.2f}"


# ============== COMMAND FACTORIES ==============

def deposit_command(account: BankAccount, amount: float) -> Command:
    """Create a deposit command."""
    def execute():
        account.deposit(amount)
    
    def undo():
        account.withdraw(amount)
    
    return make_command(
        execute, undo, 
        f"Deposit ${amount:.2f} to {account.owner}"
    )


def withdraw_command(account: BankAccount, amount: float) -> Command:
    """Create a withdraw command."""
    # Capture whether the operation succeeded
    success = [False]
    
    def execute():
        success[0] = account.withdraw(amount)
    
    def undo():
        if success[0]:
            account.deposit(amount)
    
    return make_command(
        execute, undo,
        f"Withdraw ${amount:.2f} from {account.owner}"
    )


def transfer_command(
    from_account: BankAccount, 
    to_account: BankAccount, 
    amount: float
) -> Command:
    """Create a transfer command."""
    success = [False]
    
    def execute():
        success[0] = from_account.withdraw(amount)
        if success[0]:
            to_account.deposit(amount)
    
    def undo():
        if success[0]:
            to_account.withdraw(amount)
            from_account.deposit(amount)
    
    return make_command(
        execute, undo,
        f"Transfer ${amount:.2f} from {from_account.owner} to {to_account.owner}"
    )


# ============== INVOKER ==============

class TransactionManager:
    """Manages and executes banking transactions."""
    
    def __init__(self):
        self._history: deque = deque()
        self._redo_stack: deque = deque()
    
    def execute(self, command: Command):
        """Execute a command."""
        execute_fn, undo_fn, desc = command
        print(f"→ {desc}")
        execute_fn()
        self._history.append(command)
        self._redo_stack.clear()
    
    def undo(self) -> bool:
        """Undo the last command."""
        if self._history:
            command = self._history.pop()
            _, undo_fn, desc = command
            print(f"↩ Undoing: {desc}")
            undo_fn()
            self._redo_stack.append(command)
            return True
        return False
    
    def redo(self) -> bool:
        """Redo the last undone command."""
        if self._redo_stack:
            command = self._redo_stack.pop()
            execute_fn, _, desc = command
            print(f"↪ Redoing: {desc}")
            execute_fn()
            self._history.append(command)
            return True
        return False


# ============== LAMBDA COMMANDS ==============

def simple_command(action: Callable, description: str = "") -> Command:
    """Create a command that can't be undone."""
    return make_command(action, lambda: None, description)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Command Pattern - Function-Based (Pythonic)")
    print("=" * 60)
    
    # Create accounts
    alice = BankAccount("Alice", 1000)
    bob = BankAccount("Bob", 500)
    
    print(f"\nInitial state:")
    print(f"  {alice}")
    print(f"  {bob}")
    
    # Create transaction manager
    manager = TransactionManager()
    
    # Execute transactions
    print("\n--- Executing Transactions ---")
    
    manager.execute(deposit_command(alice, 200))
    print(f"  {alice}")
    
    manager.execute(withdraw_command(bob, 100))
    print(f"  {bob}")
    
    manager.execute(transfer_command(alice, bob, 300))
    print(f"  {alice}")
    print(f"  {bob}")
    
    # Undo transactions
    print("\n--- Undo Operations ---")
    
    manager.undo()  # Undo transfer
    print(f"  {alice}")
    print(f"  {bob}")
    
    manager.undo()  # Undo Bob's withdrawal
    print(f"  {bob}")
    
    # Redo
    print("\n--- Redo Operations ---")
    manager.redo()
    print(f"  {bob}")
    
    # Lambda command example
    print("\n--- Lambda Command ---")
    
    def log_action():
        print("  📝 Logging activity...")
    
    manager.execute(simple_command(log_action, "Log activity"))
```

---

### Example 3: Command Queue

```python
#!/usr/bin/env python3
"""
Command Pattern - Command Queue

Queue commands for batch execution.
Useful for task scheduling and job processing.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Any, Optional
from datetime import datetime
from collections import deque
from enum import Enum, auto
import time


# ============== COMMAND STATUS ==============

class CommandStatus(Enum):
    PENDING = auto()
    RUNNING = auto()
    COMPLETED = auto()
    FAILED = auto()
    CANCELLED = auto()


# ============== COMMAND INTERFACE ==============

@dataclass
class QueuedCommand(ABC):
    """Command with metadata for queue management."""
    id: str = field(default_factory=lambda: f"cmd-{id(object())}")
    priority: int = 0
    status: CommandStatus = CommandStatus.PENDING
    created_at: datetime = field(default_factory=datetime.now)
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    result: Any = None
    error: Optional[str] = None
    
    @abstractmethod
    def execute(self) -> Any:
        """Execute the command."""
        pass
    
    @property
    @abstractmethod
    def description(self) -> str:
        pass
    
    def run(self) -> Any:
        """Run with status tracking."""
        self.status = CommandStatus.RUNNING
        self.started_at = datetime.now()
        
        try:
            self.result = self.execute()
            self.status = CommandStatus.COMPLETED
        except Exception as e:
            self.error = str(e)
            self.status = CommandStatus.FAILED
            raise
        finally:
            self.completed_at = datetime.now()
        
        return self.result
    
    @property
    def duration(self) -> Optional[float]:
        if self.started_at and self.completed_at:
            return (self.completed_at - self.started_at).total_seconds()
        return None


# ============== CONCRETE COMMANDS ==============

@dataclass
class PrintCommand(QueuedCommand):
    """Command that prints a message."""
    message: str = ""
    
    @property
    def description(self) -> str:
        return f"Print: {self.message}"
    
    def execute(self) -> str:
        print(f"  📢 {self.message}")
        return self.message


@dataclass
class SleepCommand(QueuedCommand):
    """Command that simulates work."""
    seconds: float = 1.0
    
    @property
    def description(self) -> str:
        return f"Sleep {self.seconds}s"
    
    def execute(self) -> None:
        print(f"  😴 Sleeping {self.seconds}s...")
        time.sleep(self.seconds)


@dataclass
class ComputeCommand(QueuedCommand):
    """Command that performs computation."""
    expression: str = "1+1"
    
    @property
    def description(self) -> str:
        return f"Compute: {self.expression}"
    
    def execute(self) -> Any:
        result = eval(self.expression)
        print(f"  🔢 {self.expression} = {result}")
        return result


@dataclass
class FailingCommand(QueuedCommand):
    """Command that always fails."""
    error_message: str = "Intentional failure"
    
    @property
    def description(self) -> str:
        return f"Fail: {self.error_message}"
    
    def execute(self) -> None:
        raise RuntimeError(self.error_message)


# ============== COMMAND QUEUE ==============

class CommandQueue:
    """
    Queue that manages command execution.
    
    Supports priority ordering, batch execution, and status tracking.
    """
    
    def __init__(self):
        self._pending: List[QueuedCommand] = []
        self._completed: List[QueuedCommand] = []
        self._failed: List[QueuedCommand] = []
    
    def enqueue(self, command: QueuedCommand) -> str:
        """Add command to queue, return command ID."""
        self._pending.append(command)
        # Sort by priority (higher first)
        self._pending.sort(key=lambda c: c.priority, reverse=True)
        print(f"📥 Queued: {command.description} (ID: {command.id})")
        return command.id
    
    def execute_next(self) -> Optional[QueuedCommand]:
        """Execute next command in queue."""
        if not self._pending:
            return None
        
        command = self._pending.pop(0)
        print(f"\n▶ Executing: {command.description}")
        
        try:
            command.run()
            self._completed.append(command)
            print(f"  ✓ Completed in {command.duration:.3f}s")
        except Exception as e:
            self._failed.append(command)
            print(f"  ✗ Failed: {e}")
        
        return command
    
    def execute_all(self) -> tuple:
        """Execute all pending commands."""
        executed = 0
        while self._pending:
            self.execute_next()
            executed += 1
        return (len(self._completed), len(self._failed))
    
    def cancel(self, command_id: str) -> bool:
        """Cancel a pending command."""
        for cmd in self._pending:
            if cmd.id == command_id:
                cmd.status = CommandStatus.CANCELLED
                self._pending.remove(cmd)
                print(f"🚫 Cancelled: {cmd.description}")
                return True
        return False
    
    def status(self) -> dict:
        """Get queue status."""
        return {
            "pending": len(self._pending),
            "completed": len(self._completed),
            "failed": len(self._failed),
        }
    
    def get_results(self) -> List[Any]:
        """Get results from completed commands."""
        return [cmd.result for cmd in self._completed]


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Command Pattern - Command Queue")
    print("=" * 60)
    
    queue = CommandQueue()
    
    # Queue various commands
    print("\n--- Queueing Commands ---")
    
    queue.enqueue(PrintCommand(message="Hello from queue!"))
    queue.enqueue(ComputeCommand(expression="2 ** 10"))
    queue.enqueue(SleepCommand(seconds=0.1))
    queue.enqueue(PrintCommand(message="High priority!", priority=10))
    queue.enqueue(ComputeCommand(expression="sum(range(100))", priority=5))
    queue.enqueue(FailingCommand(error_message="Something went wrong"))
    
    print(f"\nQueue status: {queue.status()}")
    
    # Execute all
    print("\n--- Executing Queue ---")
    completed, failed = queue.execute_all()
    
    print(f"\n--- Final Status ---")
    print(f"Completed: {completed}")
    print(f"Failed: {failed}")
    print(f"Results: {queue.get_results()}")
```

---

### Example 4: Keyboard Shortcuts / Hotkeys

```python
#!/usr/bin/env python3
"""
Command Pattern - Keyboard Shortcuts

Mapping keyboard shortcuts to commands.
Classic use case in GUI applications.
"""

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional
from abc import ABC, abstractmethod


# ============== APPLICATION (RECEIVER) ==============

class Application:
    """Simple application that commands operate on."""
    
    def __init__(self):
        self.clipboard = ""
        self.selection = ""
        self.document = "Hello, World!"
        self.cursor = len(self.document)
    
    def copy(self):
        """Copy selection to clipboard."""
        self.clipboard = self.selection
        print(f"📋 Copied: '{self.clipboard}'")
    
    def paste(self):
        """Paste clipboard at cursor."""
        before = self.document[:self.cursor]
        after = self.document[self.cursor:]
        self.document = before + self.clipboard + after
        self.cursor += len(self.clipboard)
        print(f"📄 Pasted: '{self.clipboard}'")
    
    def cut(self):
        """Cut selection to clipboard."""
        self.copy()
        # In real app, would also delete selection
        print(f"✂️ Cut: '{self.selection}'")
    
    def select_all(self):
        """Select all text."""
        self.selection = self.document
        print(f"🔲 Selected all: '{self.selection}'")
    
    def save(self):
        """Save document."""
        print(f"💾 Saved document: '{self.document}'")
    
    def new_document(self):
        """Create new document."""
        self.document = ""
        self.cursor = 0
        self.selection = ""
        print("📝 Created new document")
    
    def undo(self):
        """Undo (placeholder)."""
        print("↩ Undo")
    
    def redo(self):
        """Redo (placeholder)."""
        print("↪ Redo")
    
    def print_preview(self):
        """Show print preview."""
        print(f"🖨️ Print preview: '{self.document}'")


# ============== COMMAND CLASSES ==============

class Command(ABC):
    """Abstract command."""
    
    @abstractmethod
    def execute(self) -> None:
        pass
    
    @property
    @abstractmethod
    def name(self) -> str:
        pass


@dataclass
class CopyCommand(Command):
    app: Application
    name: str = "Copy"
    
    def execute(self):
        self.app.copy()


@dataclass
class PasteCommand(Command):
    app: Application
    name: str = "Paste"
    
    def execute(self):
        self.app.paste()


@dataclass
class CutCommand(Command):
    app: Application
    name: str = "Cut"
    
    def execute(self):
        self.app.cut()


@dataclass
class SelectAllCommand(Command):
    app: Application
    name: str = "Select All"
    
    def execute(self):
        self.app.select_all()


@dataclass
class SaveCommand(Command):
    app: Application
    name: str = "Save"
    
    def execute(self):
        self.app.save()


@dataclass
class NewCommand(Command):
    app: Application
    name: str = "New"
    
    def execute(self):
        self.app.new_document()


@dataclass
class UndoCommand(Command):
    app: Application
    name: str = "Undo"
    
    def execute(self):
        self.app.undo()


@dataclass
class RedoCommand(Command):
    app: Application
    name: str = "Redo"
    
    def execute(self):
        self.app.redo()


@dataclass
class PrintCommand(Command):
    app: Application
    name: str = "Print"
    
    def execute(self):
        self.app.print_preview()


# ============== KEYBOARD SHORTCUT MANAGER ==============

class ShortcutManager:
    """Manages keyboard shortcut to command mappings."""
    
    def __init__(self):
        self._shortcuts: Dict[str, Command] = {}
        self._history: List[Command] = []
    
    def bind(self, shortcut: str, command: Command):
        """Bind a shortcut to a command."""
        self._shortcuts[shortcut] = command
        print(f"🔗 Bound {shortcut} -> {command.name}")
    
    def unbind(self, shortcut: str):
        """Remove a shortcut binding."""
        if shortcut in self._shortcuts:
            del self._shortcuts[shortcut]
    
    def press(self, shortcut: str) -> bool:
        """Simulate pressing a keyboard shortcut."""
        if shortcut in self._shortcuts:
            command = self._shortcuts[shortcut]
            print(f"\n⌨️ {shortcut} pressed:")
            command.execute()
            self._history.append(command)
            return True
        print(f"⚠️ Unknown shortcut: {shortcut}")
        return False
    
    def show_shortcuts(self):
        """Display all shortcuts."""
        print("\n📋 Keyboard Shortcuts:")
        for shortcut, command in sorted(self._shortcuts.items()):
            print(f"  {shortcut:<15} {command.name}")


# ============== MENU (ALSO USES COMMANDS) ==============

@dataclass
class MenuItem:
    """Menu item with associated command."""
    label: str
    command: Command
    shortcut: Optional[str] = None
    
    def click(self):
        """Simulate clicking the menu item."""
        print(f"\n🖱️ Menu clicked: {self.label}")
        self.command.execute()


class Menu:
    """Simple menu structure."""
    
    def __init__(self, name: str):
        self.name = name
        self.items: List[MenuItem] = []
    
    def add_item(self, item: MenuItem):
        self.items.append(item)
    
    def show(self):
        print(f"\n📁 {self.name} Menu:")
        for item in self.items:
            shortcut_hint = f" ({item.shortcut})" if item.shortcut else ""
            print(f"  • {item.label}{shortcut_hint}")


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Command Pattern - Keyboard Shortcuts")
    print("=" * 60)
    
    # Create application
    app = Application()
    
    # Create commands
    copy_cmd = CopyCommand(app)
    paste_cmd = PasteCommand(app)
    cut_cmd = CutCommand(app)
    select_all_cmd = SelectAllCommand(app)
    save_cmd = SaveCommand(app)
    new_cmd = NewCommand(app)
    undo_cmd = UndoCommand(app)
    redo_cmd = RedoCommand(app)
    print_cmd = PrintCommand(app)
    
    # Create shortcut manager
    shortcuts = ShortcutManager()
    
    # Bind shortcuts (same commands used by menu)
    print("\n--- Setting up shortcuts ---")
    shortcuts.bind("Ctrl+C", copy_cmd)
    shortcuts.bind("Ctrl+V", paste_cmd)
    shortcuts.bind("Ctrl+X", cut_cmd)
    shortcuts.bind("Ctrl+A", select_all_cmd)
    shortcuts.bind("Ctrl+S", save_cmd)
    shortcuts.bind("Ctrl+N", new_cmd)
    shortcuts.bind("Ctrl+Z", undo_cmd)
    shortcuts.bind("Ctrl+Y", redo_cmd)
    shortcuts.bind("Ctrl+P", print_cmd)
    
    # Create menu (same commands)
    edit_menu = Menu("Edit")
    edit_menu.add_item(MenuItem("Undo", undo_cmd, "Ctrl+Z"))
    edit_menu.add_item(MenuItem("Redo", redo_cmd, "Ctrl+Y"))
    edit_menu.add_item(MenuItem("Cut", cut_cmd, "Ctrl+X"))
    edit_menu.add_item(MenuItem("Copy", copy_cmd, "Ctrl+C"))
    edit_menu.add_item(MenuItem("Paste", paste_cmd, "Ctrl+V"))
    edit_menu.add_item(MenuItem("Select All", select_all_cmd, "Ctrl+A"))
    
    # Show menu
    edit_menu.show()
    
    # Show all shortcuts
    shortcuts.show_shortcuts()
    
    # Simulate keyboard usage
    print("\n--- Simulating Keyboard Usage ---")
    shortcuts.press("Ctrl+A")  # Select all
    shortcuts.press("Ctrl+C")  # Copy
    shortcuts.press("Ctrl+V")  # Paste
    shortcuts.press("Ctrl+S")  # Save
    
    # Same operation via menu
    print("\n--- Simulating Menu Usage ---")
    edit_menu.items[4].click()  # Click "Paste"
```

---

## 6. When to Use / When to Avoid

### Use When:
- Need to **parameterize** objects with actions
- Need to **queue** or **log** requests
- Need **undoable** operations
- Want to decouple **invocation** from **execution**

### Avoid When:
- Simple direct method calls suffice
- No need for undo/redo/queue functionality
- Adding command layer adds unnecessary complexity

### Python Idiom:
For simple cases, use **functions** or **lambdas**:

```python
# Commands as functions
commands = {
    "save": lambda: doc.save(),
    "copy": lambda: doc.copy(),
}

commands["save"]()  # Execute
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Memento** | Works with Command for undo/redo |
| **Strategy** | Similar structure; Command focuses on "what" not "how" |
| **Chain of Responsibility** | Commands can be passed along a chain |
| **Composite** | Macro commands are composites of commands |

