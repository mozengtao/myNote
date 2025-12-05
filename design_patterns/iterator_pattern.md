# Iterator Pattern (迭代器模式)

## ASCII Diagram

```
+-------------------+          +-------------------+
|     Iterable      |          |     Iterator      |<<interface>>
+-------------------+          +-------------------+
| +__iter__()       |--------->| +__next__()       |
+-------------------+          | +__iter__()       |
         ^                     +-------------------+
         |                              ^
         |                              |
+--------+--------+           +--------+--------+
|ConcreteIterable |           |ConcreteIterator |
+-----------------+           +-----------------+
| - data          |           | - collection    |
+-----------------+           | - position      |
| +__iter__()     |           +-----------------+
+-----------------+           | +__next__()     |
                              +-----------------+

Iteration Flow:
+------------+     __iter__()     +------------+     __next__()     +-------+
| Collection |------------------>| Iterator   |------------------>| Item  |
+------------+                    +------------+                    +-------+
      |                                 |                               |
      |                                 | (maintains position)          |
      |                                 v                               |
      |                           +-----------+                         |
      |                           | position=0|----> Item 1            |
      |                           | position=1|----> Item 2            |
      |                           | position=2|----> Item 3            |
      |                           | ...       |                         |
      |                           +-----------+                         |
      |                                 |                               |
      |                                 v                               |
      |                           StopIteration                         |
      +---------------------------------------------------------------+
```

**中文说明：**
- **Iterable（可迭代对象）**：定义返回迭代器的方法 `__iter__()`
- **Iterator（迭代器）**：定义访问元素的方法 `__next__()` 和 `__iter__()`
- **ConcreteIterable（具体可迭代对象）**：存储数据的集合
- **ConcreteIterator（具体迭代器）**：实现遍历逻辑
- **关键点**：分离集合的存储和遍历逻辑

---

## 核心思想

提供一种方法**顺序访问**一个聚合对象中的各个元素，而又**不暴露**该对象的内部表示。迭代器模式将遍历逻辑从集合中分离出来，封装到独立的迭代器对象中。

---

## 应用场景

1. **访问集合内容**：需要访问集合对象的内容而不暴露其内部结构
2. **统一遍历接口**：为不同类型的集合提供统一的遍历接口
3. **多种遍历方式**：需要对同一集合提供多种遍历方式
4. **实际应用**：
   - 数据库游标
   - 文件系统遍历
   - 分页数据获取
   - 树/图的遍历
   - 流式数据处理

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 单一职责 | 遍历逻辑独立于集合 |
| 开闭原则 | 可以添加新的迭代器而不修改集合 |
| 并行遍历 | 每个迭代器独立维护状态 |
| 延迟计算 | 可以按需生成元素 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 过度设计 | 简单集合可能不需要 |
| 效率 | 可能比直接访问稍慢 |

---

## Python 代码示例

### 应用前：暴露内部结构

```python
# 问题：直接暴露内部数据结构

class BookCollection:
    def __init__(self):
        self._books = []  # 内部使用列表
    
    def add(self, book):
        self._books.append(book)
    
    def get_books(self):
        return self._books  # 暴露内部结构


class MagazineCollection:
    def __init__(self):
        self._magazines = {}  # 内部使用字典
    
    def add(self, id, magazine):
        self._magazines[id] = magazine
    
    def get_magazines(self):
        return self._magazines  # 暴露不同的内部结构


# 客户端代码需要知道内部结构
books = BookCollection()
books.add("Book 1")
books.add("Book 2")

# 遍历列表
for book in books.get_books():
    print(book)

magazines = MagazineCollection()
magazines.add("M1", "Magazine 1")
magazines.add("M2", "Magazine 2")

# 遍历字典 - 不同的方式
for id, mag in magazines.get_magazines().items():
    print(f"{id}: {mag}")

# 问题：
# 1. 暴露了内部数据结构
# 2. 不同集合遍历方式不统一
# 3. 更换数据结构需要修改客户端代码
```

### 应用后：使用迭代器模式

```python
from abc import ABC, abstractmethod
from typing import Any, Iterator, Generic, TypeVar, List, Optional
from collections.abc import Iterable

T = TypeVar('T')


# ========== 迭代器接口 ==========
class BookIterator(ABC):
    """书籍迭代器抽象基类"""
    
    @abstractmethod
    def __iter__(self):
        pass
    
    @abstractmethod
    def __next__(self):
        pass
    
    @abstractmethod
    def has_next(self) -> bool:
        pass


# ========== 具体迭代器 ==========
class ForwardIterator(BookIterator):
    """正向迭代器"""
    
    def __init__(self, collection: List[Any]):
        self._collection = collection
        self._position = 0
    
    def __iter__(self):
        return self
    
    def __next__(self):
        if not self.has_next():
            raise StopIteration
        item = self._collection[self._position]
        self._position += 1
        return item
    
    def has_next(self) -> bool:
        return self._position < len(self._collection)


class ReverseIterator(BookIterator):
    """反向迭代器"""
    
    def __init__(self, collection: List[Any]):
        self._collection = collection
        self._position = len(collection) - 1
    
    def __iter__(self):
        return self
    
    def __next__(self):
        if not self.has_next():
            raise StopIteration
        item = self._collection[self._position]
        self._position -= 1
        return item
    
    def has_next(self) -> bool:
        return self._position >= 0


class FilterIterator(BookIterator):
    """过滤迭代器"""
    
    def __init__(self, collection: List[Any], predicate):
        self._collection = collection
        self._predicate = predicate
        self._position = 0
        self._advance_to_next_valid()
    
    def _advance_to_next_valid(self):
        while (self._position < len(self._collection) and 
               not self._predicate(self._collection[self._position])):
            self._position += 1
    
    def __iter__(self):
        return self
    
    def __next__(self):
        if not self.has_next():
            raise StopIteration
        item = self._collection[self._position]
        self._position += 1
        self._advance_to_next_valid()
        return item
    
    def has_next(self) -> bool:
        return self._position < len(self._collection)


class SkipIterator(BookIterator):
    """跳跃迭代器（每隔 N 个取一个）"""
    
    def __init__(self, collection: List[Any], step: int = 2):
        self._collection = collection
        self._step = step
        self._position = 0
    
    def __iter__(self):
        return self
    
    def __next__(self):
        if not self.has_next():
            raise StopIteration
        item = self._collection[self._position]
        self._position += self._step
        return item
    
    def has_next(self) -> bool:
        return self._position < len(self._collection)


# ========== 具体集合 ==========
class Book:
    """书籍类"""
    
    def __init__(self, title: str, author: str, year: int, genre: str):
        self.title = title
        self.author = author
        self.year = year
        self.genre = genre
    
    def __repr__(self):
        return f"Book('{self.title}' by {self.author}, {self.year})"


class BookCollection:
    """书籍集合 - 支持多种迭代方式"""
    
    def __init__(self):
        self._books: List[Book] = []
    
    def add(self, book: Book):
        self._books.append(book)
    
    def __len__(self):
        return len(self._books)
    
    def __iter__(self):
        """默认正向迭代"""
        return ForwardIterator(self._books)
    
    def reverse_iterator(self) -> BookIterator:
        """反向迭代"""
        return ReverseIterator(self._books)
    
    def filter_by(self, predicate) -> BookIterator:
        """过滤迭代"""
        return FilterIterator(self._books, predicate)
    
    def skip_iterator(self, step: int = 2) -> BookIterator:
        """跳跃迭代"""
        return SkipIterator(self._books, step)
    
    # 便捷的过滤方法
    def by_author(self, author: str) -> BookIterator:
        return self.filter_by(lambda b: b.author == author)
    
    def by_genre(self, genre: str) -> BookIterator:
        return self.filter_by(lambda b: b.genre == genre)
    
    def by_year_range(self, start: int, end: int) -> BookIterator:
        return self.filter_by(lambda b: start <= b.year <= end)


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    # 创建书籍集合
    collection = BookCollection()
    collection.add(Book("1984", "George Orwell", 1949, "Dystopian"))
    collection.add(Book("To Kill a Mockingbird", "Harper Lee", 1960, "Fiction"))
    collection.add(Book("The Great Gatsby", "F. Scott Fitzgerald", 1925, "Fiction"))
    collection.add(Book("Pride and Prejudice", "Jane Austen", 1813, "Romance"))
    collection.add(Book("The Catcher in the Rye", "J.D. Salinger", 1951, "Fiction"))
    collection.add(Book("Brave New World", "Aldous Huxley", 1932, "Dystopian"))
    
    print("=" * 60)
    print("1. Default Forward Iteration")
    print("=" * 60)
    for book in collection:
        print(f"  {book}")
    
    print("\n" + "=" * 60)
    print("2. Reverse Iteration")
    print("=" * 60)
    for book in collection.reverse_iterator():
        print(f"  {book}")
    
    print("\n" + "=" * 60)
    print("3. Filter by Genre (Fiction)")
    print("=" * 60)
    for book in collection.by_genre("Fiction"):
        print(f"  {book}")
    
    print("\n" + "=" * 60)
    print("4. Filter by Year Range (1930-1960)")
    print("=" * 60)
    for book in collection.by_year_range(1930, 1960):
        print(f"  {book}")
    
    print("\n" + "=" * 60)
    print("5. Skip Iterator (every 2nd book)")
    print("=" * 60)
    for book in collection.skip_iterator(2):
        print(f"  {book}")
    
    print("\n" + "=" * 60)
    print("6. Multiple Independent Iterators")
    print("=" * 60)
    iter1 = iter(collection)
    iter2 = iter(collection)
    
    print(f"  Iterator 1, first: {next(iter1)}")
    print(f"  Iterator 1, second: {next(iter1)}")
    print(f"  Iterator 2, first: {next(iter2)}")  # 独立的位置


# ========== Python 生成器实现 ==========
print("\n" + "=" * 60)
print("7. Generator-based Iterators (Pythonic)")
print("=" * 60)


class PythonicBookCollection:
    """使用生成器实现迭代器"""
    
    def __init__(self):
        self._books: List[Book] = []
    
    def add(self, book: Book):
        self._books.append(book)
    
    def __iter__(self):
        """默认迭代器（生成器）"""
        for book in self._books:
            yield book
    
    def reverse(self):
        """反向迭代（生成器）"""
        for book in reversed(self._books):
            yield book
    
    def filter(self, predicate):
        """过滤迭代（生成器）"""
        for book in self._books:
            if predicate(book):
                yield book
    
    def paginate(self, page_size: int):
        """分页迭代（生成器）"""
        for i in range(0, len(self._books), page_size):
            yield self._books[i:i + page_size]
    
    def with_index(self):
        """带索引迭代"""
        for i, book in enumerate(self._books):
            yield i, book


# 使用 Pythonic 集合
pythonic = PythonicBookCollection()
pythonic.add(Book("Book A", "Author 1", 2020, "Tech"))
pythonic.add(Book("Book B", "Author 2", 2021, "Tech"))
pythonic.add(Book("Book C", "Author 1", 2022, "Fiction"))
pythonic.add(Book("Book D", "Author 3", 2023, "Tech"))

print("\nPaginated (page_size=2):")
for page_num, page in enumerate(pythonic.paginate(2), 1):
    print(f"  Page {page_num}: {page}")

print("\nWith index:")
for idx, book in pythonic.with_index():
    print(f"  [{idx}] {book}")


# ========== 树的迭代器 ==========
print("\n" + "=" * 60)
print("8. Tree Iterators (DFS/BFS)")
print("=" * 60)


class TreeNode:
    def __init__(self, value):
        self.value = value
        self.children: List['TreeNode'] = []
    
    def add_child(self, child: 'TreeNode'):
        self.children.append(child)
        return self


class Tree:
    def __init__(self, root: TreeNode):
        self.root = root
    
    def dfs_preorder(self):
        """深度优先 - 前序遍历"""
        def traverse(node):
            yield node.value
            for child in node.children:
                yield from traverse(child)
        yield from traverse(self.root)
    
    def dfs_postorder(self):
        """深度优先 - 后序遍历"""
        def traverse(node):
            for child in node.children:
                yield from traverse(child)
            yield node.value
        yield from traverse(self.root)
    
    def bfs(self):
        """广度优先遍历"""
        from collections import deque
        queue = deque([self.root])
        while queue:
            node = queue.popleft()
            yield node.value
            queue.extend(node.children)
    
    def __iter__(self):
        """默认使用 DFS 前序"""
        return self.dfs_preorder()


# 构建树
#       A
#      /|\
#     B C D
#    /|   |
#   E F   G

root = TreeNode("A")
b = TreeNode("B")
c = TreeNode("C")
d = TreeNode("D")
b.add_child(TreeNode("E")).add_child(TreeNode("F"))
d.add_child(TreeNode("G"))
root.add_child(b).add_child(c).add_child(d)

tree = Tree(root)

print("\nDFS Preorder:")
print("  " + " -> ".join(tree.dfs_preorder()))

print("\nDFS Postorder:")
print("  " + " -> ".join(tree.dfs_postorder()))

print("\nBFS:")
print("  " + " -> ".join(tree.bfs()))


# ========== 无限迭代器 ==========
print("\n" + "=" * 60)
print("9. Infinite Iterators")
print("=" * 60)


def fibonacci():
    """斐波那契数列（无限）"""
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b


def take(n: int, iterable):
    """取前 N 个元素"""
    for i, item in enumerate(iterable):
        if i >= n:
            break
        yield item


print("First 10 Fibonacci numbers:")
print("  " + ", ".join(str(x) for x in take(10, fibonacci())))


def cycle(iterable):
    """无限循环"""
    saved = []
    for item in iterable:
        yield item
        saved.append(item)
    while saved:
        for item in saved:
            yield item


print("\nCycling through [A, B, C] (first 8):")
print("  " + ", ".join(take(8, cycle(["A", "B", "C"]))))
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **封装性** | 暴露内部数据结构 | 隐藏内部实现 |
| **统一接口** | 不同集合遍历方式不同 | 统一的迭代接口 |
| **多种遍历** | 难以实现多种遍历 | 可以提供多种迭代器 |
| **并行遍历** | 共享遍历状态 | 每个迭代器独立 |
| **灵活性** | 更换结构影响客户端 | 更换结构不影响遍历 |

---

## Python 迭代器协议

```python
# Python 迭代器协议
class MyIterator:
    def __iter__(self):
        return self
    
    def __next__(self):
        # 返回下一个元素或抛出 StopIteration
        pass

# 可迭代对象协议
class MyIterable:
    def __iter__(self):
        return MyIterator()

# 生成器（最简洁的方式）
def my_generator():
    yield 1
    yield 2
    yield 3
```

---

## 与其他模式的关系

| 模式 | 关系 |
|------|------|
| **Composite** | 迭代器常用于遍历组合结构 |
| **Factory Method** | 集合可以使用工厂方法创建适当的迭代器 |
| **Memento** | 可以使用迭代器遍历备忘录历史 |

