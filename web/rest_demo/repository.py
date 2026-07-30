"""
Repository 层
=============
对应架构图中：

    Service -> Repository -> Database

Repository 是 Service 和 Database 之间的"数据访问抽象层"：
Service 只调用 BookRepository 的方法，完全不知道底层是 dict、SQL 还是别的存储。
这正是文档第九节表格里 "Repository / DAO：数据访问" 这一职责的体现——
Service 永远不会直接碰 Database，只通过 Repository。
"""

from typing import Optional

from database import Database
from model import Book


class BookRepository:
    def __init__(self, db: Database):
        # 依赖注入：Repository 持有一个 Database 实例，负责把业务对象翻译成存储操作。
        self._db = db

    def add(self, title: str, author: str) -> Book:
        return self._db.insert(title, author)

    def find_by_id(self, book_id: int) -> Optional[Book]:
        return self._db.get(book_id)

    def find_all(self) -> list[Book]:
        return self._db.list_all()

    def save(self, book: Book) -> Book:
        return self._db.update(book)

    def remove(self, book_id: int) -> bool:
        return self._db.delete(book_id)
