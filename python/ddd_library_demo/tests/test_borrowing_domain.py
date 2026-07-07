"""
Tests demonstrating the `Library` Aggregate Root: its consistency
boundary, invariants, and event recording.
"""

from __future__ import annotations

from datetime import date
from uuid import uuid4

import pytest

from borrowing.domain.entities import (
    BookNotInCirculationError,
    CopiesAvailableError,
    Library,
    LoanAlreadyReturnedError,
    LoanNotFoundError,
    NoAvailableCopiesError,
)
from borrowing.domain.events import (
    BookAddedToCirculation,
    BookBorrowed,
    BookOverdue,
    BookReturned,
    ReservationCreated,
    ReservationFulfilled,
)
from borrowing.domain.value_objects import ISBN, LibraryId, LoanId, MemberId, ReservationId

TODAY = date(2026, 1, 1)


def make_library(copies: int = 1) -> tuple[Library, ISBN]:
    library = Library(LibraryId(uuid4()), name="Test Library")
    isbn = ISBN("9780321125217")
    library.add_to_circulation(isbn, copies)
    library.pull_events()  # discard BookAddedToCirculation for cleaner test bodies
    return library, isbn


class TestCirculation:
    def test_add_to_circulation_records_event(self) -> None:
        library = Library(LibraryId(uuid4()), name="Test Library")
        isbn = ISBN("9780321125217")

        library.add_to_circulation(isbn, copies=3)

        assert library.available_copies(isbn) == 3
        events = library.pull_events()
        assert any(isinstance(event, BookAddedToCirculation) for event in events)

    def test_adding_more_copies_of_known_isbn_accumulates(self) -> None:
        library, isbn = make_library(copies=1)
        library.add_to_circulation(isbn, copies=2)
        assert library.available_copies(isbn) == 3


class TestBorrowing:
    def test_borrow_decrements_available_copies(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        assert library.available_copies(isbn) == 0

    def test_borrow_records_book_borrowed_event(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        events = library.pull_events()
        assert any(isinstance(event, BookBorrowed) for event in events)

    def test_borrow_fails_when_no_copies_available(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)

        with pytest.raises(NoAvailableCopiesError):
            library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)

    def test_borrow_fails_for_unknown_isbn(self) -> None:
        library = Library(LibraryId(uuid4()), name="Test Library")
        with pytest.raises(BookNotInCirculationError):
            library.borrow_book(ISBN("9780134494166"), MemberId(uuid4()), LoanId(uuid4()), TODAY)


class TestReturningAndReservations:
    def test_reserve_fails_when_copies_are_available(self) -> None:
        library, isbn = make_library(copies=1)
        with pytest.raises(CopiesAvailableError):
            library.reserve_book(isbn, MemberId(uuid4()), ReservationId(uuid4()), TODAY)

    def test_reserve_succeeds_when_no_copies_available(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)

        reservation = library.reserve_book(isbn, MemberId(uuid4()), ReservationId(uuid4()), TODAY)

        assert reservation.fulfilled is False
        assert any(
            isinstance(event, ReservationCreated) for event in library.pull_events()
        )

    def test_return_fulfills_oldest_pending_reservation_fifo(self) -> None:
        library, isbn = make_library(copies=1)
        alice_loan = library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        bob_id = MemberId(uuid4())
        library.reserve_book(isbn, bob_id, ReservationId(uuid4()), TODAY)
        library.pull_events()

        library.return_book(alice_loan.id, today=TODAY, next_loan_id=LoanId(uuid4()))

        # The copy went straight to Bob (FIFO), not back into the general pool.
        assert library.available_copies(isbn) == 0
        assert any(r.member_id == bob_id and r.fulfilled for r in library.reservations)
        events = library.pull_events()
        assert any(isinstance(event, BookReturned) for event in events)
        assert any(isinstance(event, ReservationFulfilled) for event in events)
        assert any(isinstance(event, BookBorrowed) for event in events)

    def test_return_without_pending_reservation_frees_the_copy(self) -> None:
        library, isbn = make_library(copies=1)
        loan = library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)

        library.return_book(loan.id, today=TODAY)

        assert library.available_copies(isbn) == 1

    def test_returning_unknown_loan_raises(self) -> None:
        library, _ = make_library(copies=1)
        with pytest.raises(LoanNotFoundError):
            library.return_book(LoanId(uuid4()), today=TODAY)

    def test_returning_same_loan_twice_raises(self) -> None:
        library, isbn = make_library(copies=1)
        loan = library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        library.return_book(loan.id, today=TODAY)

        with pytest.raises(LoanAlreadyReturnedError):
            library.return_book(loan.id, today=TODAY)


class TestOverdueLoans:
    def test_find_overdue_loans_flags_and_records_events(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        library.pull_events()

        far_future = date(2026, 3, 1)
        overdue = library.find_overdue_loans(far_future)

        assert len(overdue) == 1
        assert any(isinstance(event, BookOverdue) for event in library.pull_events())

    def test_no_overdue_loans_when_within_period(self) -> None:
        library, isbn = make_library(copies=1)
        library.borrow_book(isbn, MemberId(uuid4()), LoanId(uuid4()), TODAY)
        assert library.find_overdue_loans(TODAY) == []
