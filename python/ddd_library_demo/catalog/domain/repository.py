"""
Catalog / Domain / Repository interface

DDD Concept: Repository (interface lives in Domain)
------------------------------------------------------
The Domain layer defines WHAT persistence operations it needs
(`save`, `get_by_id`, ...) without knowing HOW they are implemented. This
is the Dependency Inversion Principle applied to persistence: high-level
Domain/Application code depends only on this abstraction; the concrete
`infrastructure.in_memory_repositories.InMemoryBookRepository` depends on
(implements) it - never the other way around.

Why `Protocol` instead of `abc.ABC`
--------------------------------------
`Protocol` gives structural typing - `InMemoryBookRepository` satisfies
`BookRepository` simply by having matching methods, with no import of, or
inheritance from, this module required at runtime. That keeps
`infrastructure/` free to depend on `catalog/domain/`, while
`catalog/domain/` still gets full static type-checking of anything typed
as `BookRepository`.
"""

from __future__ import annotations

from typing import Protocol

from catalog.domain.entities import Book
from catalog.domain.value_objects import BookId, ISBN


class BookNotFoundError(Exception):
    """Raised when a repository lookup finds no matching Book."""


class BookRepository(Protocol):
    def save(self, book: Book) -> None:
        """Persist the current state of `book` (insert or update)."""
        ...

    def get_by_id(self, book_id: BookId) -> Book:
        """Return the Book with `book_id`, or raise BookNotFoundError."""
        ...

    def get_by_isbn(self, isbn: ISBN) -> Book:
        """Return the Book with `isbn`, or raise BookNotFoundError."""
        ...

    def list_all(self) -> list[Book]:
        """Return every Book currently known to the catalog."""
        ...
