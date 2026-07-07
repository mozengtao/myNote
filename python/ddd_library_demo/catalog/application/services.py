"""
Catalog / Application / Services

DDD Concept: Application Service
------------------------------------
Each service below implements exactly one use case. Its job is purely to
ORCHESTRATE: load/construct the aggregate, invoke its behaviour, persist
it, then publish whatever domain events the aggregate recorded. Compare
this to `Book.retire()` in `catalog/domain/entities.py`, which is where
the actual business rule ("cannot retire twice") is enforced - the
Application Service here has no idea what makes a retirement valid, it
just asks the aggregate to do it and reacts to the outcome.
"""

from __future__ import annotations

import logging

from catalog.domain.entities import Book
from catalog.domain.repository import BookRepository
from catalog.domain.value_objects import AuthorName, BookId, BookTitle, ISBN
from shared.domain_event import EventBus
from shared.id_generator import IdGenerator

logger = logging.getLogger(__name__)


class RegisterBookService:
    """Use case: add a brand-new title to the catalog."""

    def __init__(
        self,
        book_repository: BookRepository,
        event_bus: EventBus,
        id_generator: IdGenerator,
    ) -> None:
        # All three collaborators are received as interfaces (Protocols),
        # never as concrete Infrastructure classes - Dependency Inversion
        # in action. `main.py` is the one place that wires concrete
        # implementations in.
        self._book_repository = book_repository
        self._event_bus = event_bus
        self._id_generator = id_generator

    def register_book(
        self, isbn: str, title: str, author: str, copies: int = 1
    ) -> Book:
        print(f"[Catalog] Registering book '{title}' ({isbn})...")
        book = Book.register(
            book_id=BookId(self._id_generator.new_id()),
            isbn=ISBN(isbn),
            title=BookTitle(title),
            author=AuthorName(author),
            initial_copies=copies,
        )
        self._book_repository.save(book)
        logger.info("Book registered: %r", book)

        # Publishing AFTER save guarantees subscribers only ever react to
        # state that has actually been persisted.
        for event in book.pull_events():
            self._event_bus.publish(event)
        print(f"[Catalog] Book registered and BookRegistered event published.")
        return book


class RetireBookService:
    """Use case: withdraw a title from the catalog."""

    def __init__(self, book_repository: BookRepository, event_bus: EventBus) -> None:
        self._book_repository = book_repository
        self._event_bus = event_bus

    def retire_book(self, book_id: BookId) -> Book:
        print(f"[Catalog] Retiring book {book_id}...")
        book = self._book_repository.get_by_id(book_id)
        book.retire()
        self._book_repository.save(book)

        for event in book.pull_events():
            self._event_bus.publish(event)
        print(f"[Catalog] Book {book_id} retired.")
        return book
