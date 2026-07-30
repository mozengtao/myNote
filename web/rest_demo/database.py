"""
Database 层
===========
对应架构图最底层的那个方块：

    Repository -> Database

真实项目里这里会是 MySQL / PostgreSQL / MongoDB 等持久化存储。
这个 demo 为了聚焦在"分层结构"本身，用一个进程内的 dict 模拟数据库表
（重启进程数据会丢失，这是刻意的简化，不是 bug）。
"""

from typing import Optional

from model import Book


class Database:
    """模拟一张 `books` 表：{id: Book}。"""

    def __init__(self):
        # _table 就是"表"；_next_id 模拟数据库的自增主键。
        self._table: dict[int, Book] = {}
        self._next_id = 1

    def insert(self, title: str, author: str) -> Book:
        book = Book(id=self._next_id, title=title, author=author)
        self._table[book.id] = book
        self._next_id += 1
        print(f"[Database] INSERT books -> id={book.id}")  # 落到"数据库"这一层
        return book

    def get(self, book_id: int) -> Optional[Book]:
        book = self._table.get(book_id)
        print(f"[Database] SELECT books WHERE id={book_id} -> {'FOUND' if book else 'NOT FOUND'}")
        return book

    def list_all(self) -> list[Book]:
        print(f"[Database] SELECT * FROM books -> {len(self._table)} row(s)")
        return list(self._table.values())

    def update(self, book: Book) -> Book:
        # 整条记录覆盖写回，对应 PUT 语义（整体替换资源）。
        self._table[book.id] = book
        print(f"[Database] UPDATE books SET ... WHERE id={book.id}")
        return book

    def delete(self, book_id: int) -> bool:
        existed = book_id in self._table
        self._table.pop(book_id, None)
        print(f"[Database] DELETE FROM books WHERE id={book_id} -> {'OK' if existed else 'NOT FOUND'}")
        return existed
