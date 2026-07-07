"""
Infrastructure / In-memory Repository implementations

DDD Concept: Dependency Inversion at the persistence seam
-------------------------------------------------------------
Both classes below implement their respective Domain-defined `Protocol`
(`catalog.domain.repository.BookRepository` and
`borrowing.domain.repository.LibraryRepository`) using nothing more than a
plain `dict`. No real database, no ORM, no SQL - as required by the spec.
Because the Domain/Application layers only ever type-hint against the
`Protocol`, swapping either of these for a real database-backed repository
later would not require changing anything outside this one file.
"""

from __future__ import annotations

from catalog.domain.entities import Book
from catalog.domain.repository import BookNotFoundError
from catalog.domain.value_objects import BookId, ISBN
from borrowing.domain.entities import Library
from borrowing.domain.repository import LibraryNotFoundError
from borrowing.domain.value_objects import LibraryId


class InMemoryBookRepository:
    """In-memory stand-in for a real Catalog persistence store."""

    def __init__(self) -> None:
        self._books: dict[BookId, Book] = {}

    def save(self, book: Book) -> None:
        self._books[book.id] = book

    def get_by_id(self, book_id: BookId) -> Book:
        try:
            return self._books[book_id]
        except KeyError as exc:
            raise BookNotFoundError(f"No book with id {book_id}") from exc

    def get_by_isbn(self, isbn: ISBN) -> Book:
        for book in self._books.values():
            if book.isbn == isbn:
                return book
        raise BookNotFoundError(f"No book with isbn {isbn}")

    def list_all(self) -> list[Book]:
        return list(self._books.values())


class InMemoryLibraryRepository:
    """In-memory stand-in for a real Borrowing persistence store."""

    def __init__(self) -> None:
        self._libraries: dict[LibraryId, Library] = {}

    def save(self, library: Library) -> None:
        self._libraries[library.id] = library

    def get_by_id(self, library_id: LibraryId) -> Library:
        try:
            return self._libraries[library_id]
        except KeyError as exc:
            raise LibraryNotFoundError(f"No library with id {library_id}") from exc

    def list_all(self) -> list[Library]:
        return list(self._libraries.values())
