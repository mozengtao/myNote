# Iterator Pattern in Python

---

## 1. Pattern Name: Iterator

**Purpose / Problem Solved:**
Provide a way to access elements of a collection sequentially without exposing underlying representation. Python has native support via `__iter__` and `__next__`.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                     ITERABLE (Collection)                         |
|------------------------------------------------------------------|
| + __iter__() -> Iterator                                          |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                         ITERATOR                                  |
|------------------------------------------------------------------|
| - current_position                                                |
|------------------------------------------------------------------|
| + __iter__() -> self                                              |
| + __next__() -> Element   # Raises StopIteration when done        |
+------------------------------------------------------------------+

Python's for loop:
    for item in collection:
        process(item)

Is equivalent to:
    iterator = iter(collection)   # calls __iter__
    while True:
        try:
            item = next(iterator)  # calls __next__
            process(item)
        except StopIteration:
            break
```

**中文说明：**
迭代器模式提供顺序访问集合元素的方式，无需暴露底层结构。Python原生支持：实现`__iter__`返回迭代器，`__next__`返回下一元素并在结束时抛出StopIteration。更简单的方式是使用生成器（yield）。

---

## 3. Python Grammar & Tips

| Feature | Usage |
|---------|-------|
| **`__iter__`** | Return iterator object |
| **`__next__`** | Return next item or raise StopIteration |
| **`yield`** | Generator-based iterator (simplest) |
| **`yield from`** | Delegate to sub-iterator |
| **`itertools`** | Standard library iterator utilities |

---

## 4. Python Module Example

```python
#!/usr/bin/env python3
"""Iterator Pattern - Multiple Implementation Methods"""

from typing import Iterator, Iterable


# ============== METHOD 1: Class-based Iterator ==============

class Range:
    """Custom range implementation."""
    
    def __init__(self, start: int, end: int, step: int = 1):
        self.start = start
        self.end = end
        self.step = step
    
    def __iter__(self):
        return RangeIterator(self.start, self.end, self.step)


class RangeIterator:
    """Separate iterator class."""
    
    def __init__(self, start: int, end: int, step: int):
        self.current = start
        self.end = end
        self.step = step
    
    def __iter__(self):
        return self
    
    def __next__(self):
        if self.current >= self.end:
            raise StopIteration
        value = self.current
        self.current += self.step
        return value


# ============== METHOD 2: Generator Function ==============

def fibonacci(n: int) -> Iterator[int]:
    """Fibonacci sequence generator."""
    a, b = 0, 1
    for _ in range(n):
        yield a
        a, b = b, a + b


def infinite_counter(start: int = 0) -> Iterator[int]:
    """Infinite counter - use with care!"""
    current = start
    while True:
        yield current
        current += 1


# ============== METHOD 3: __iter__ as Generator ==============

class TreeNode:
    """Binary tree with iterator."""
    
    def __init__(self, value, left=None, right=None):
        self.value = value
        self.left = left
        self.right = right
    
    def __iter__(self):
        """In-order traversal using generator."""
        if self.left:
            yield from self.left
        yield self.value
        if self.right:
            yield from self.right


# ============== METHOD 4: Reverse Iterator ==============

class ReversibleList:
    """List that supports reverse iteration."""
    
    def __init__(self, data):
        self._data = list(data)
    
    def __iter__(self):
        return iter(self._data)
    
    def __reversed__(self):
        return reversed(self._data)


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Class-based Iterator ===")
    for i in Range(0, 5):
        print(i, end=" ")
    print()
    
    print("\n=== Generator Function ===")
    for f in fibonacci(10):
        print(f, end=" ")
    print()
    
    print("\n=== Tree Iterator ===")
    tree = TreeNode(4,
        TreeNode(2, TreeNode(1), TreeNode(3)),
        TreeNode(6, TreeNode(5), TreeNode(7))
    )
    print("In-order:", list(tree))
    
    print("\n=== Reverse Iterator ===")
    rlist = ReversibleList([1, 2, 3, 4, 5])
    print("Forward:", list(rlist))
    print("Reverse:", list(reversed(rlist)))
    
    print("\n=== Infinite Iterator (limited) ===")
    from itertools import islice
    print(list(islice(infinite_counter(10), 5)))
```

---

## 5. When to Use

**Use When:**
- Need to traverse a collection
- Want to hide internal structure
- Need multiple traversal algorithms

**Python Approach:**
Prefer generators (`yield`) for simplicity. Use class-based iterators when you need to maintain state between iterations.

---

## 6. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Composite** | Iterators traverse composite structures |
| **Factory Method** | Polymorphic iterator creation |
| **Visitor** | Alternative for complex traversal logic |

