"""
Service 层
==========
对应架构图中：

    Controller -> Service -> Repository

Service 承载"业务逻辑"（文档第九节表格：Service = 业务逻辑），
和只做增删改查搬运的 Repository 不同，Service 里会有真正的规则判断。
`update_status` 是最典型的例子：它实现了文档第三节 State（状态）里
"Book: available <-> borrowed" 的状态机，并且不允许非法的状态跃迁
（比如借一本已经被借走的书），这就是 Repository 层做不到、必须放在 Service 层的东西。
"""

from typing import Optional

from model import Book
from repository import BookRepository

# 合法状态集合 + 合法跃迁规则，对应文档 "State（状态）" 一节的 Book 状态机。
VALID_STATUSES = {"available", "borrowed"}


class BookNotFoundError(Exception):
    """资源不存在 -> Controller 会把它映射成 HTTP 404。"""


class InvalidStatusTransitionError(Exception):
    """非法状态跃迁（例如重复借阅） -> Controller 会把它映射成 HTTP 409。"""


class BookService:
    def __init__(self, repository: BookRepository):
        self._repo = repository

    def create_book(self, title: str, author: str) -> Book:
        print(f"[Service] create_book(title={title!r}, author={author!r})")
        return self._repo.add(title, author)

    def list_books(self) -> list[Book]:
        print("[Service] list_books()")
        return self._repo.find_all()

    def get_book(self, book_id: int) -> Book:
        print(f"[Service] get_book(id={book_id})")
        book = self._repo.find_by_id(book_id)
        if book is None:
            raise BookNotFoundError(f"book {book_id} not found")
        return book

    def replace_book(self, book_id: int, title: str, author: str, status: str) -> Book:
        """对应 PUT：整体替换资源。"""
        print(f"[Service] replace_book(id={book_id})")
        if self._repo.find_by_id(book_id) is None:
            raise BookNotFoundError(f"book {book_id} not found")
        if status not in VALID_STATUSES:
            raise ValueError(f"invalid status: {status!r}")
        updated = Book(id=book_id, title=title, author=author, status=status)
        return self._repo.save(updated)

    def update_status(self, book_id: int, new_status: str) -> Book:
        """对应 PATCH：局部更新资源状态（借书 / 还书）。

        这里是文档 3.4 节 "Transfer（状态转移）" 的直接实现：
        客户端发一个 Representation（{"status": "borrowed"}），
        服务器据此修改 Resource State，这就是"状态转移"。
        """
        print(f"[Service] update_status(id={book_id}, new_status={new_status!r})")
        if new_status not in VALID_STATUSES:
            raise ValueError(f"invalid status: {new_status!r}")

        book = self._repo.find_by_id(book_id)
        if book is None:
            raise BookNotFoundError(f"book {book_id} not found")

        # 业务规则：不能借一本已经被借走的书（也不能"归还"一本本来就可借的书）。
        if book.status == new_status:
            raise InvalidStatusTransitionError(
                f"book {book_id} is already {book.status!r}"
            )

        book.status = new_status
        return self._repo.save(book)

    def delete_book(self, book_id: int) -> None:
        print(f"[Service] delete_book(id={book_id})")
        if not self._repo.remove(book_id):
            raise BookNotFoundError(f"book {book_id} not found")
