# Memento Pattern (备忘录模式)

## ASCII Diagram

```
+-------------------+     +-------------------+     +-------------------+
|    Originator     |     |      Memento      |     |    Caretaker      |
+-------------------+     +-------------------+     +-------------------+
| - state           |     | - state           |     | - mementos[]      |
+-------------------+     +-------------------+     +-------------------+
| + createMemento() |---->| + getState()      |<----| + backup()        |
| + restore(m)      |     +-------------------+     | + undo()          |
+-------------------+                               | + redo()          |
                                                    +-------------------+

State History:
+------------+     +------------+     +------------+     +------------+
| Memento 1  |<--->| Memento 2  |<--->| Memento 3  |<--->| Memento 4  |
+------------+     +------------+     +------------+     +------------+
| state: A   |     | state: B   |     | state: C   |     | state: D   |
+------------+     +------------+     +------------+     +------------+
                                           ^
                                           |
                                       Current

Backup/Restore Flow:
+-------------+      createMemento()      +---------+      add to history     +------------+
| Originator  |-------------------------->| Memento |------------------------>| Caretaker  |
+-------------+                           +---------+                         +------------+
      ^                                                                              |
      |                     restore(memento)                                         |
      +------------------------------------------------------------------------------+
```

**中文说明：**
- **Originator（发起人）**：需要保存和恢复状态的对象
- **Memento（备忘录）**：存储发起人的内部状态
- **Caretaker（管理者）**：负责保存和管理备忘录，不能修改备忘录内容
- **关键点**：在不破坏封装的前提下，捕获对象的内部状态

---

## 核心思想

在**不破坏封装性**的前提下，捕获一个对象的**内部状态**，并在该对象之外保存这个状态，以便以后可以将对象恢复到原先保存的状态。备忘录模式实现了对象状态的快照和恢复。

---

## 应用场景

1. **撤销/重做**：需要保存和恢复对象状态
2. **快照**：需要保存对象在某一时刻的状态
3. **事务回滚**：操作失败时恢复到之前的状态
4. **实际应用**：
   - 文本编辑器撤销
   - 游戏存档
   - 数据库事务
   - 表单数据恢复
   - 虚拟机快照

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 封装性 | 不暴露对象实现细节 |
| 简化发起人 | 状态管理交给管理者 |
| 撤销支持 | 容易实现撤销和恢复 |
| 状态快照 | 可以保存多个历史状态 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 内存消耗 | 保存大量状态可能消耗内存 |
| 性能 | 频繁创建备忘录可能影响性能 |
| 维护成本 | 管理者需要管理备忘录的生命周期 |

---

## Python 代码示例

### 应用前：直接暴露状态

```python
# 问题：文本编辑器的撤销功能，直接复制整个对象

class TextEditor:
    """文本编辑器 - 没有备忘录"""
    
    def __init__(self):
        self.content = ""
        self.cursor_position = 0
        self.selection_start = None
        self.selection_end = None
    
    def type(self, text):
        self.content = (self.content[:self.cursor_position] + 
                       text + 
                       self.content[self.cursor_position:])
        self.cursor_position += len(text)
    
    def get_content(self):
        return self.content


# 手动管理历史
history = []
editor = TextEditor()

# 保存状态 - 直接复制所有属性（破坏封装）
def save_state():
    history.append({
        'content': editor.content,
        'cursor': editor.cursor_position,
        'sel_start': editor.selection_start,
        'sel_end': editor.selection_end,
    })

def undo():
    if history:
        state = history.pop()
        editor.content = state['content']
        editor.cursor_position = state['cursor']
        editor.selection_start = state['sel_start']
        editor.selection_end = state['sel_end']

# 问题：
# 1. 外部代码需要知道所有内部状态
# 2. 状态增加时需要修改保存逻辑
# 3. 破坏了封装性
```

### 应用后：使用备忘录模式

```python
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Optional, Any, Dict
from datetime import datetime
import copy
import json


# ========== 备忘录 ==========
@dataclass
class EditorMemento:
    """
    编辑器备忘录
    
    存储编辑器的状态快照，对外不可变
    """
    _content: str
    _cursor_position: int
    _selection: Optional[tuple]
    _timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    _description: str = ""
    
    # 只提供读取接口，保证不可变
    @property
    def content(self) -> str:
        return self._content
    
    @property
    def cursor_position(self) -> int:
        return self._cursor_position
    
    @property
    def selection(self) -> Optional[tuple]:
        return self._selection
    
    @property
    def timestamp(self) -> str:
        return self._timestamp
    
    @property
    def description(self) -> str:
        return self._description
    
    def __str__(self):
        preview = self._content[:30] + "..." if len(self._content) > 30 else self._content
        return f"[{self._timestamp}] '{preview}' ({self._description})"


# ========== 发起人 ==========
class TextEditor:
    """
    文本编辑器 - 发起人
    
    创建和恢复备忘录
    """
    
    def __init__(self):
        self._content = ""
        self._cursor_position = 0
        self._selection: Optional[tuple] = None
    
    # ===== 编辑操作 =====
    def type_text(self, text: str) -> None:
        """输入文本"""
        if self._selection:
            # 如果有选区，替换选区内容
            start, end = self._selection
            self._content = self._content[:start] + text + self._content[end:]
            self._cursor_position = start + len(text)
            self._selection = None
        else:
            # 在光标位置插入
            self._content = (self._content[:self._cursor_position] + 
                           text + 
                           self._content[self._cursor_position:])
            self._cursor_position += len(text)
    
    def delete(self, count: int = 1) -> str:
        """删除字符"""
        if self._selection:
            start, end = self._selection
            deleted = self._content[start:end]
            self._content = self._content[:start] + self._content[end:]
            self._cursor_position = start
            self._selection = None
        else:
            start = max(0, self._cursor_position - count)
            deleted = self._content[start:self._cursor_position]
            self._content = self._content[:start] + self._content[self._cursor_position:]
            self._cursor_position = start
        return deleted
    
    def move_cursor(self, position: int) -> None:
        """移动光标"""
        self._cursor_position = max(0, min(position, len(self._content)))
        self._selection = None
    
    def select(self, start: int, end: int) -> None:
        """选择文本"""
        self._selection = (max(0, start), min(end, len(self._content)))
    
    def select_all(self) -> None:
        """全选"""
        self._selection = (0, len(self._content))
    
    # ===== 备忘录操作 =====
    def create_memento(self, description: str = "") -> EditorMemento:
        """创建备忘录（快照）"""
        return EditorMemento(
            _content=self._content,
            _cursor_position=self._cursor_position,
            _selection=self._selection,
            _description=description
        )
    
    def restore(self, memento: EditorMemento) -> None:
        """从备忘录恢复状态"""
        self._content = memento.content
        self._cursor_position = memento.cursor_position
        self._selection = memento.selection
    
    # ===== 属性 =====
    @property
    def content(self) -> str:
        return self._content
    
    @property
    def cursor_position(self) -> int:
        return self._cursor_position
    
    def __str__(self) -> str:
        # 显示内容和光标位置
        before = self._content[:self._cursor_position]
        after = self._content[self._cursor_position:]
        return f"'{before}|{after}'"


# ========== 管理者 ==========
class EditorHistory:
    """
    编辑器历史管理者
    
    管理备忘录的保存和恢复
    """
    
    def __init__(self, editor: TextEditor, max_history: int = 100):
        self._editor = editor
        self._history: List[EditorMemento] = []
        self._redo_stack: List[EditorMemento] = []
        self._max_history = max_history
    
    def backup(self, description: str = "") -> None:
        """保存当前状态"""
        memento = self._editor.create_memento(description)
        self._history.append(memento)
        self._redo_stack.clear()  # 新操作清空重做栈
        
        # 限制历史长度
        if len(self._history) > self._max_history:
            self._history.pop(0)
        
        print(f"  [History] Saved: {memento}")
    
    def undo(self) -> bool:
        """撤销"""
        if len(self._history) < 2:
            print("  [History] Nothing to undo")
            return False
        
        # 当前状态入重做栈
        current = self._history.pop()
        self._redo_stack.append(current)
        
        # 恢复到上一个状态
        previous = self._history[-1]
        self._editor.restore(previous)
        print(f"  [History] Undo to: {previous}")
        return True
    
    def redo(self) -> bool:
        """重做"""
        if not self._redo_stack:
            print("  [History] Nothing to redo")
            return False
        
        memento = self._redo_stack.pop()
        self._history.append(memento)
        self._editor.restore(memento)
        print(f"  [History] Redo to: {memento}")
        return True
    
    def show_history(self) -> None:
        """显示历史记录"""
        print("\n  History:")
        for i, m in enumerate(self._history):
            marker = " <- current" if i == len(self._history) - 1 else ""
            print(f"    {i+1}. {m}{marker}")
        if self._redo_stack:
            print("  Redo stack:")
            for i, m in enumerate(self._redo_stack):
                print(f"    {i+1}. {m}")
    
    @property
    def can_undo(self) -> bool:
        return len(self._history) >= 2
    
    @property
    def can_redo(self) -> bool:
        return len(self._redo_stack) > 0


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    print("=" * 60)
    print("Text Editor with Memento Pattern")
    print("=" * 60)
    
    editor = TextEditor()
    history = EditorHistory(editor)
    
    # 保存初始状态
    history.backup("Initial")
    
    # 编辑操作
    print("\n--- Editing ---")
    editor.type_text("Hello")
    print(f"After 'Hello': {editor}")
    history.backup("Type 'Hello'")
    
    editor.type_text(" World")
    print(f"After ' World': {editor}")
    history.backup("Type ' World'")
    
    editor.type_text("!")
    print(f"After '!': {editor}")
    history.backup("Type '!'")
    
    # 撤销
    print("\n--- Undo ---")
    history.undo()
    print(f"After undo: {editor}")
    
    history.undo()
    print(f"After undo: {editor}")
    
    # 重做
    print("\n--- Redo ---")
    history.redo()
    print(f"After redo: {editor}")
    
    # 新操作（清空重做栈）
    print("\n--- New operation (clears redo) ---")
    editor.type_text("??")
    print(f"After '??': {editor}")
    history.backup("Type '??'")
    
    # 尝试重做（应该失败）
    history.redo()
    
    # 显示历史
    history.show_history()


# ========== 游戏存档示例 ==========
print("\n" + "=" * 60)
print("Game Save System with Memento")
print("=" * 60)


@dataclass
class GameMemento:
    """游戏备忘录"""
    level: int
    health: int
    position: tuple
    inventory: List[str]
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    save_name: str = ""
    
    def to_dict(self) -> dict:
        return {
            'level': self.level,
            'health': self.health,
            'position': self.position,
            'inventory': self.inventory,
            'timestamp': self.timestamp,
            'save_name': self.save_name
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'GameMemento':
        return cls(**data)


class GameCharacter:
    """游戏角色 - 发起人"""
    
    def __init__(self, name: str):
        self.name = name
        self._level = 1
        self._health = 100
        self._position = (0, 0)
        self._inventory: List[str] = []
    
    def level_up(self):
        self._level += 1
        self._health = 100 + self._level * 10
        print(f"  {self.name} leveled up to {self._level}!")
    
    def take_damage(self, damage: int):
        self._health = max(0, self._health - damage)
        print(f"  {self.name} took {damage} damage, health: {self._health}")
    
    def move_to(self, x: int, y: int):
        self._position = (x, y)
        print(f"  {self.name} moved to {self._position}")
    
    def pick_up(self, item: str):
        self._inventory.append(item)
        print(f"  {self.name} picked up {item}")
    
    def create_save(self, save_name: str = "") -> GameMemento:
        """创建存档"""
        return GameMemento(
            level=self._level,
            health=self._health,
            position=self._position,
            inventory=self._inventory.copy(),
            save_name=save_name
        )
    
    def load_save(self, memento: GameMemento):
        """加载存档"""
        self._level = memento.level
        self._health = memento.health
        self._position = memento.position
        self._inventory = memento.inventory.copy()
    
    def __str__(self):
        return (f"{self.name}: Lv.{self._level}, HP:{self._health}, "
                f"Pos:{self._position}, Items:{self._inventory}")


class SaveManager:
    """存档管理器 - 管理者"""
    
    def __init__(self):
        self._saves: Dict[str, GameMemento] = {}
        self._auto_saves: List[GameMemento] = []
        self._max_auto_saves = 3
    
    def quick_save(self, character: GameCharacter, slot: str = "quick"):
        """快速存档"""
        save = character.create_save(f"Quick Save - {slot}")
        self._saves[slot] = save
        print(f"  [SaveManager] Quick saved to slot '{slot}'")
    
    def quick_load(self, character: GameCharacter, slot: str = "quick") -> bool:
        """快速读档"""
        if slot not in self._saves:
            print(f"  [SaveManager] No save in slot '{slot}'")
            return False
        character.load_save(self._saves[slot])
        print(f"  [SaveManager] Loaded from slot '{slot}'")
        return True
    
    def auto_save(self, character: GameCharacter):
        """自动存档"""
        save = character.create_save("Auto Save")
        self._auto_saves.append(save)
        if len(self._auto_saves) > self._max_auto_saves:
            self._auto_saves.pop(0)
        print(f"  [SaveManager] Auto saved ({len(self._auto_saves)}/{self._max_auto_saves})")
    
    def load_auto_save(self, character: GameCharacter, index: int = -1) -> bool:
        """加载自动存档"""
        if not self._auto_saves:
            print("  [SaveManager] No auto saves")
            return False
        save = self._auto_saves[index]
        character.load_save(save)
        print(f"  [SaveManager] Loaded auto save from {save.timestamp}")
        return True
    
    def list_saves(self):
        """列出所有存档"""
        print("\n  Available saves:")
        for slot, save in self._saves.items():
            print(f"    [{slot}] {save.save_name} - Lv.{save.level} @ {save.timestamp}")
        print(f"  Auto saves: {len(self._auto_saves)}")


# 使用游戏存档
hero = GameCharacter("Hero")
save_manager = SaveManager()

print("\n--- Playing ---")
hero.level_up()
hero.pick_up("Sword")
hero.move_to(10, 20)
print(f"Status: {hero}")

save_manager.quick_save(hero, "before_boss")
save_manager.auto_save(hero)

print("\n--- Boss fight (took damage) ---")
hero.take_damage(50)
hero.move_to(15, 25)
print(f"Status: {hero}")

print("\n--- Oops, load quick save ---")
save_manager.quick_load(hero, "before_boss")
print(f"Status: {hero}")

save_manager.list_saves()
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **封装性** | 外部代码知道所有内部状态 | 状态封装在备忘录中 |
| **职责分离** | 对象自己管理历史 | 管理者负责历史管理 |
| **维护性** | 状态变化需修改保存逻辑 | 备忘录自动适应 |
| **扩展性** | 难以添加新功能 | 易于添加撤销、重做等 |

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **Command** | 常与备忘录一起实现撤销 |
| **Iterator** | 可以遍历备忘录历史 |
| **Prototype** | 备忘录通常通过克隆创建 |

---

## 注意事项

```python
# 1. 深拷贝 vs 浅拷贝
class Memento:
    def __init__(self, state):
        # 使用深拷贝确保独立
        self._state = copy.deepcopy(state)

# 2. 增量备忘录（节省内存）
class IncrementalMemento:
    def __init__(self, changes: dict):
        self._changes = changes  # 只保存变化

# 3. 压缩历史
class CompressedHistory:
    def compress(self):
        # 合并连续的小修改
        pass
```

