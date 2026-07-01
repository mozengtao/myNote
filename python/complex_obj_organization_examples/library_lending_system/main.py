"""
图书馆借阅系统 —— 完整可运行示例

本示例重点演示的心智模型原则（对应 complex_obj_organization.md 第十七节 Checklist）：
    1. Object Graph        —— Library 拥有 Book / Member 集合，形成对象图，而不是几个孤立的列表/字典
    2. Encapsulation        —— Book 自己管理"是否被借出"这个状态，只能通过 borrow()/return_book() 修改
    3. Repository           —— LibraryRepository 负责按 ISBN / 会员号查找对象，业务代码不直接操作底层 dict/list

运行方式：
    python3 main.py
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import date


# ---------------------------------------------------------------------------
# 一、Domain Object：先建对象，而不是先写函数
# ---------------------------------------------------------------------------

class Book:
    """一本书。只描述"书是什么"，不知道 Library、不知道数据库。"""

    def __init__(self, isbn: str, title: str, author: str):
        self.isbn = isbn
        self.title = title
        self.author = author
        # 借阅状态是 Book 自己的状态，外部不应该直接改
        self._borrowed_by: "Member | None" = None

    @property
    def is_borrowed(self) -> bool:
        return self._borrowed_by is not None

    def borrow(self, member: "Member") -> None:
        """对象自己管理自己的状态：由 Book 校验是否可以被借，而不是外部直接赋值。"""
        if self.is_borrowed:
            raise ValueError(f"《{self.title}》已被 {self._borrowed_by.name} 借出，不能重复借阅")
        self._borrowed_by = member

    def return_book(self) -> None:
        if not self.is_borrowed:
            raise ValueError(f"《{self.title}》当前不在借出状态，无法归还")
        self._borrowed_by = None

    def __repr__(self) -> str:
        status = f"已借出给 {self._borrowed_by.name}" if self.is_borrowed else "在架"
        return f"<Book {self.title!r} ({self.isbn}) - {status}>"


class Member:
    """一个会员。拥有自己的借阅记录（Loan 列表），而不是把记录散落在全局变量里。"""

    def __init__(self, member_id: str, name: str):
        self.member_id = member_id
        self.name = name
        # Has-A：Member 拥有 Loan 集合（组合），而不是继承一个 "Borrower" 基类
        self.loans: list["Loan"] = []

    def add_loan(self, loan: "Loan") -> None:
        """对象负责维护自己的关联集合，而不是让外部到处 member.loans.append(...)。"""
        self.loans.append(loan)

    def active_loans(self) -> list["Loan"]:
        return [loan for loan in self.loans if not loan.returned]

    def __repr__(self) -> str:
        return f"<Member {self.name} ({self.member_id})>"


@dataclass
class Loan:
    """一次借阅记录：把 Book 和 Member 关联起来，形成 Object Graph 里的一条"边"。"""

    book: Book
    member: Member
    borrowed_on: date
    returned: bool = field(default=False)
    returned_on: date | None = field(default=None)

    def close(self, returned_on: date) -> None:
        self.returned = True
        self.returned_on = returned_on


# ---------------------------------------------------------------------------
# 二、Library：把 Book / Member 组合起来，形成 Object Graph
# ---------------------------------------------------------------------------

class Library:
    """
    Library 拥有 Book 集合与 Member 集合（Composition / Has-A）。

    Library 本身只描述"图书馆有哪些书、哪些会员、发生过哪些借阅"，
    不知道 SSH、不知道数据库、不知道 HTTP —— 那些是 Infrastructure 层的事。
    """

    def __init__(self, name: str):
        self.name = name
        self.books: dict[str, Book] = {}
        self.members: dict[str, Member] = {}
        self.loans: list[Loan] = []

    def add_book(self, book: Book) -> None:
        self.books[book.isbn] = book

    def add_member(self, member: Member) -> None:
        self.members[member.member_id] = member

    def borrow_book(self, isbn: str, member_id: str, today: date) -> Loan:
        """
        隐藏内部结构：调用方只需要 isbn + member_id，
        不需要知道 Library 内部是用 dict 存的书和会员（Hide Structure）。
        """
        book = self.books[isbn]
        member = self.members[member_id]

        book.borrow(member)  # 委托给 Book 自己校验状态
        loan = Loan(book=book, member=member, borrowed_on=today)
        member.add_loan(loan)
        self.loans.append(loan)
        return loan

    def return_book(self, isbn: str, today: date) -> Loan:
        book = self.books[isbn]
        book.return_book()

        # 找到这本书当前未归还的那条借阅记录并关闭
        active_loan = next(
            loan for loan in self.loans
            if loan.book is book and not loan.returned
        )
        active_loan.close(returned_on=today)
        return active_loan


# ---------------------------------------------------------------------------
# 三、Repository：集中管理对象的查找方式，业务代码不直接摸底层集合
# ---------------------------------------------------------------------------

class LibraryRepository:
    """
    Repository 只负责"根据某个 key 找到对象"，
    调用方永远不应该写 library.books.values()[3] 这种依赖内部实现细节的代码。
    """

    def __init__(self, library: Library):
        self._library = library

    def find_book_by_isbn(self, isbn: str) -> Book:
        return self._library.books[isbn]

    def find_member_by_id(self, member_id: str) -> Member:
        return self._library.members[member_id]

    def overdue_loans(self, today: date, max_days: int = 14) -> list[Loan]:
        """业务规则查询也应该封装在 Repository / Service 里，而不是让调用方自己遍历、自己算天数。"""
        return [
            loan for loan in self._library.loans
            if not loan.returned and (today - loan.borrowed_on).days > max_days
        ]


# ---------------------------------------------------------------------------
# 四、演示：对象在各层之间流动，而不是退化成 dict
# ---------------------------------------------------------------------------

def main() -> None:
    library = Library(name="社区图书馆")

    # Object First：先创建领域对象，而不是先拼一个 books = [...] 的 dict 列表
    book_a = Book(isbn="978-1", title="Clean Code", author="Robert C. Martin")
    book_b = Book(isbn="978-2", title="Refactoring", author="Martin Fowler")
    library.add_book(book_a)
    library.add_book(book_b)

    alice = Member(member_id="M001", name="Alice")
    bob = Member(member_id="M002", name="Bob")
    library.add_member(alice)
    library.add_member(bob)

    repo = LibraryRepository(library)

    today = date(2026, 1, 1)
    library.borrow_book(isbn="978-1", member_id="M001", today=today)
    library.borrow_book(isbn="978-2", member_id="M002", today=today)

    print("=== 借阅后书籍状态 ===")
    for book in library.books.values():
        print(book)

    # 隐藏结构：外部通过 Repository 查找，而不是直接翻底层 dict
    found = repo.find_book_by_isbn("978-1")
    print(f"\n通过 Repository 查到的书：{found}")

    print("\n=== Alice 当前借阅中的书 ===")
    for loan in alice.active_loans():
        print(f"- {loan.book.title}，借出日：{loan.borrowed_on}")

    # 归还书籍：状态变更仍然由 Book 自己校验、自己完成
    library.return_book(isbn="978-1", today=date(2026, 1, 20))
    print("\n=== 归还后书籍状态 ===")
    for book in library.books.values():
        print(book)

    # 逾期查询：业务规则封装在 Repository，调用方不需要自己写日期比较逻辑
    overdue = repo.overdue_loans(today=date(2026, 1, 20), max_days=14)
    print("\n=== 逾期未还（超过 14 天）===")
    for loan in overdue:
        days = (date(2026, 1, 20) - loan.borrowed_on).days
        print(f"- {loan.member.name} 借的《{loan.book.title}》已 {days} 天未还")


if __name__ == "__main__":
    main()
