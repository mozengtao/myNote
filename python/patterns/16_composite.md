# Composite Pattern in Python

---

## 1. Pattern Name: Composite

**Purpose / Problem Solved:**
Compose objects into tree structures to represent part-whole hierarchies. Let clients treat individual objects and compositions uniformly.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                     COMPONENT (Interface)                         |
|------------------------------------------------------------------|
| + operation()                                                     |
| + add(child)         # Only for composites                        |
| + remove(child)      # Only for composites                        |
| + get_children()     # Only for composites                        |
+------------------------------------------------------------------+
              ^                               ^
              |                               |
+-------------------------+     +-------------------------+
|         LEAF            |     |       COMPOSITE         |
|-------------------------|     |-------------------------|
| + operation()           |     | - children: List        |
| # Implements behavior   |     |-------------------------|
|                         |     | + operation()           |
|                         |     |   for child in children:|
|                         |     |     child.operation()   |
|                         |     | + add(child)            |
|                         |     | + remove(child)         |
+-------------------------+     +-------------------------+
                                         |
                          +--------------+--------------+
                          |              |              |
                       [Leaf]      [Composite]       [Leaf]
                                        |
                                  +-----+-----+
                                  |           |
                               [Leaf]      [Leaf]
```

**中文说明：**
组合模式将对象组合成树形结构表示"部分-整体"层次。Leaf是叶节点（无子节点），Composite是容器节点（有子节点）。两者实现相同接口，客户端可以统一处理单个对象和组合对象。典型应用：文件系统、GUI组件树、组织结构、菜单系统。

---

## 3. Python Module Example

```python
#!/usr/bin/env python3
"""Composite Pattern - File System Example"""

from abc import ABC, abstractmethod
from typing import List


# ============== COMPONENT ==============

class FileSystemItem(ABC):
    """Abstract component for file system items."""
    
    def __init__(self, name: str):
        self.name = name
        self.parent = None
    
    @abstractmethod
    def get_size(self) -> int:
        pass
    
    @abstractmethod
    def display(self, indent: int = 0) -> str:
        pass
    
    def get_path(self) -> str:
        if self.parent:
            return f"{self.parent.get_path()}/{self.name}"
        return self.name


# ============== LEAF ==============

class File(FileSystemItem):
    """Leaf node - represents a file."""
    
    def __init__(self, name: str, size: int):
        super().__init__(name)
        self.size = size
    
    def get_size(self) -> int:
        return self.size
    
    def display(self, indent: int = 0) -> str:
        return " " * indent + f"📄 {self.name} ({self.size}B)"


# ============== COMPOSITE ==============

class Directory(FileSystemItem):
    """Composite node - contains files and directories."""
    
    def __init__(self, name: str):
        super().__init__(name)
        self._children: List[FileSystemItem] = []
    
    def add(self, item: FileSystemItem) -> "Directory":
        item.parent = self
        self._children.append(item)
        return self
    
    def remove(self, item: FileSystemItem):
        item.parent = None
        self._children.remove(item)
    
    def get_size(self) -> int:
        """Recursively calculate total size."""
        return sum(child.get_size() for child in self._children)
    
    def display(self, indent: int = 0) -> str:
        lines = [" " * indent + f"📁 {self.name}/ ({self.get_size()}B)"]
        for child in self._children:
            lines.append(child.display(indent + 2))
        return "\n".join(lines)
    
    def find(self, name: str) -> List[FileSystemItem]:
        """Find items by name recursively."""
        results = []
        for child in self._children:
            if child.name == name:
                results.append(child)
            if isinstance(child, Directory):
                results.extend(child.find(name))
        return results
    
    def __iter__(self):
        """Iterate over children."""
        return iter(self._children)


# ============== MENU EXAMPLE ==============

class MenuItem:
    """Base menu item (can be leaf or composite)."""
    
    def __init__(self, name: str, action=None):
        self.name = name
        self.action = action
        self.children = []
    
    def add(self, item: "MenuItem") -> "MenuItem":
        self.children.append(item)
        return self
    
    def execute(self):
        if self.action:
            self.action()
    
    def display(self, indent: int = 0) -> str:
        prefix = "├── " if indent else ""
        if self.children:
            lines = [" " * indent + prefix + f"📂 {self.name}"]
            for child in self.children:
                lines.append(child.display(indent + 4))
            return "\n".join(lines)
        return " " * indent + prefix + f"📌 {self.name}"


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== File System Composite ===\n")
    
    # Build file system
    root = Directory("home")
    
    docs = Directory("documents")
    docs.add(File("resume.pdf", 102400))
    docs.add(File("cover_letter.docx", 51200))
    
    photos = Directory("photos")
    vacation = Directory("vacation")
    vacation.add(File("beach.jpg", 2048000))
    vacation.add(File("sunset.jpg", 1536000))
    photos.add(vacation)
    photos.add(File("profile.png", 512000))
    
    root.add(docs)
    root.add(photos)
    root.add(File(".bashrc", 4096))
    
    print(root.display())
    print(f"\nTotal size: {root.get_size():,} bytes")
    print(f"\nPath to beach.jpg: {vacation.find('beach.jpg')[0].get_path()}")
    
    print("\n=== Menu Composite ===\n")
    
    menu = MenuItem("Main Menu")
    
    file_menu = MenuItem("File")
    file_menu.add(MenuItem("New", lambda: print("New file")))
    file_menu.add(MenuItem("Open", lambda: print("Open file")))
    file_menu.add(MenuItem("Save", lambda: print("Save file")))
    
    edit_menu = MenuItem("Edit")
    edit_menu.add(MenuItem("Cut"))
    edit_menu.add(MenuItem("Copy"))
    edit_menu.add(MenuItem("Paste"))
    
    menu.add(file_menu)
    menu.add(edit_menu)
    menu.add(MenuItem("Help"))
    
    print(menu.display())
```

---

## 4. When to Use / Avoid

**Use When:**
- You have tree structures (part-whole hierarchies)
- Clients should treat leaf and composite uniformly
- You need recursive composition

**Avoid When:**
- Structure isn't naturally hierarchical
- Leaves and composites have very different interfaces

---

## 5. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Iterator** | Traverse composite structures |
| **Visitor** | Operations across composite tree |
| **Decorator** | Often used with Composite |

