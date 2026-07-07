"""
Tests demonstrating the `Book` Entity/Aggregate Root: identity,
encapsulated state transitions, and event recording.
"""

from __future__ import annotations

from uuid import uuid4

import pytest

from catalog.domain.entities import Book, BookAlreadyRetiredError, BookNotRetiredError, BookStatus
from catalog.domain.events import BookRegistered, BookRetired
from catalog.domain.value_objects import ISBN, AuthorName, BookId, BookTitle


def make_book() -> Book:
    return Book.register(
        book_id=BookId(uuid4()),
        isbn=ISBN("9780321125217"),
        title=BookTitle("Domain-Driven Design"),
        author=AuthorName("Eric Evans"),
        initial_copies=2,
    )


class TestBookIdentity:
    def test_two_books_with_same_attributes_but_different_ids_are_not_equal(self) -> None:
        isbn = ISBN("9780321125217")
        title = BookTitle("Domain-Driven Design")
        author = AuthorName("Eric Evans")
        book_a = Book.register(BookId(uuid4()), isbn, title, author, initial_copies=1)
        book_b = Book.register(BookId(uuid4()), isbn, title, author, initial_copies=1)

        # Same "content", different identity -> not equal. This is the
        # defining difference between an Entity and a Value Object.
        assert book_a != book_b

    def test_same_id_means_equal_regardless_of_other_state(self) -> None:
        book_id = BookId(uuid4())
        isbn = ISBN("9780321125217")
        book_a = Book(book_id, isbn, BookTitle("Title A"), AuthorName("Author A"))
        book_b = Book(book_id, isbn, BookTitle("Title B"), AuthorName("Author B"))
        assert book_a == book_b


class TestBookRegistration:
    def test_register_records_book_registered_event(self) -> None:
        book = make_book()
        events = book.pull_events()
        assert len(events) == 1
        assert isinstance(events[0], BookRegistered)
        assert events[0].copies == 2

    def test_pull_events_drains_the_queue(self) -> None:
        book = make_book()
        book.pull_events()
        assert book.pull_events() == []

    def test_new_book_starts_active(self) -> None:
        assert make_book().status is BookStatus.ACTIVE


class TestBookLifecycle:
    def test_retire_transitions_to_retired_and_records_event(self) -> None:
        book = make_book()
        book.pull_events()  # discard the registration event

        book.retire()

        assert book.status is BookStatus.RETIRED
        events = book.pull_events()
        assert any(isinstance(event, BookRetired) for event in events)

    def test_cannot_retire_twice(self) -> None:
        book = make_book()
        book.retire()
        with pytest.raises(BookAlreadyRetiredError):
            book.retire()

    def test_cannot_reactivate_a_book_that_was_never_retired(self) -> None:
        book = make_book()
        with pytest.raises(BookNotRetiredError):
            book.reactivate()

    def test_reactivate_after_retire_restores_active_status(self) -> None:
        book = make_book()
        book.retire()
        book.reactivate()
        assert book.status is BookStatus.ACTIVE
