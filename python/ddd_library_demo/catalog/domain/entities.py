"""
Catalog / Domain / Entities

DDD Concept: Aggregate Root
----------------------------
`Book` is the Aggregate Root of the Catalog context. It is small enough
here to be the ONLY object in its aggregate (no child entities), but it is
still an Aggregate Root: it is the sole gateway through which its own
state may be changed, and it is what `BookRepository` loads and saves as a
single consistency unit.
"""

from __future__ import annotations

from enum import Enum, auto

from catalog.domain.events import BookRegistered, BookRetired
from catalog.domain.value_objects import AuthorName, BookId, BookTitle, ISBN
from shared.entity import AggregateRoot


class BookStatus(Enum):
    """
    The lifecycle state of a catalog entry.

    Why an Enum instead of a bare boolean/string
    -----------------------------------------------
    A book's lifecycle could grow more states later (e.g. `OUT_OF_PRINT`).
    An Enum gives named, exhaustively-checkable states instead of magic
    strings scattered through the codebase.
    """

    ACTIVE = auto()
    RETIRED = auto()


class BookAlreadyRetiredError(Exception):
    """Raised when retiring a book that is already retired - protects the
    invariant that a retirement is a one-way, one-time transition."""


class BookNotRetiredError(Exception):
    """Raised when reactivating a book that was never retired."""


class Book(AggregateRoot[BookId]):
    """
    A bibliographic catalog entry: title, author, ISBN, and lifecycle
    status.

    Why this is an Entity, not a Value Object
    --------------------------------------------
    Two books can share the exact same title and author (e.g. two
    different editions, or simply a duplicate catalog entry created by
    mistake) yet must remain distinguishable - each has its own identity,
    its own lifecycle, and its own copies in circulation. That need for
    "sameness over time despite possibly-changing attributes" is exactly
    what makes something an Entity rather than a Value Object.

    Why it is NOT anemic
    -----------------------
    Notice there is no `book.status = BookStatus.RETIRED` anywhere in this
    codebase. State changes only happen through `retire()` /
    `reactivate()`, which are the only two places that may enforce the
    "cannot retire twice" / "cannot reactivate an active book" invariants.
    This keeps business rules colocated with the data they protect,
    instead of scattered across Application Services.
    """

    def __init__(
        self,
        book_id: BookId,
        isbn: ISBN,
        title: BookTitle,
        author: AuthorName,
        status: BookStatus = BookStatus.ACTIVE,
    ) -> None:
        super().__init__(book_id)
        self._isbn = isbn
        self._title = title
        self._author = author
        self._status = status

    @property
    def isbn(self) -> ISBN:
        return self._isbn

    @property
    def title(self) -> BookTitle:
        return self._title

    @property
    def author(self) -> AuthorName:
        return self._author

    @property
    def status(self) -> BookStatus:
        return self._status

    @classmethod
    def register(
        cls,
        book_id: BookId,
        isbn: ISBN,
        title: BookTitle,
        author: AuthorName,
        initial_copies: int,
    ) -> Book:
        """
        Factory method creating a brand-new catalog entry and recording
        the `BookRegistered` event.

        Why a factory classmethod instead of a public constructor + a
        separate "publish event" call
        ------------------------------------------------------------------
        Bundling "create the object" and "record that it was created" in
        one place makes it impossible to construct a `Book` whose creation
        was never announced to the rest of the system - there is exactly
        one path into existence for a `Book`.
        """
        book = cls(book_id=book_id, isbn=isbn, title=title, author=author)
        book._record_event(
            BookRegistered(
                book_id=book_id,
                isbn=isbn,
                title=title,
                author=author,
                copies=initial_copies,
            )
        )
        return book

    def retire(self) -> None:
        """
        Withdraw this title from the catalog.

        Why this invariant belongs on the Entity
        --------------------------------------------
        "You cannot retire an already-retired book" is a rule about THIS
        book's own state transitions - it needs no information from any
        other aggregate or external policy, which is the textbook case for
        placing a rule on the Entity itself rather than in an Application
        Service or Domain Service.
        """
        if self._status is BookStatus.RETIRED:
            raise BookAlreadyRetiredError(f"Book {self._id} is already retired")
        self._status = BookStatus.RETIRED
        self._record_event(BookRetired(book_id=self._id, isbn=self._isbn))

    def reactivate(self) -> None:
        """Bring a retired title back into the active catalog."""
        if self._status is not BookStatus.RETIRED:
            raise BookNotRetiredError(f"Book {self._id} is not retired")
        self._status = BookStatus.ACTIVE

    def __repr__(self) -> str:
        return (
            f"Book(id={self._id}, isbn={self._isbn}, title={self._title!r}, "
            f"status={self._status.name})"
        )
