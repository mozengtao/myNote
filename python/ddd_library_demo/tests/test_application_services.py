"""
End-to-end tests for the Application Services, wired against REAL
in-memory infrastructure (repositories + event bus) rather than mocks -
per the spec's "avoid mocks whenever possible" testing guideline. These
tests exercise the same object flow that `main.py` demonstrates, just
without the narration print statements.
"""

from __future__ import annotations

from datetime import date, timedelta
from uuid import uuid4

import pytest

from borrowing.application.services import (
    BorrowBookService,
    CheckOverdueLoansService,
    ReserveBookService,
    ReturnBookService,
)
from borrowing.domain.entities import Library, NoAvailableCopiesError
from borrowing.domain.value_objects import LibraryId, MemberId
from catalog.application.services import RegisterBookService, RetireBookService
from catalog.domain.entities import BookStatus
from catalog.domain.events import BookRegistered
from infrastructure.event_bus import InMemoryEventBus
from infrastructure.event_handlers import AddBookToCirculationHandler
from infrastructure.id_generator import UuidIdGenerator
from infrastructure.in_memory_repositories import (
    InMemoryBookRepository,
    InMemoryLibraryRepository,
)

ISBN = "9780321125217"


@pytest.fixture
def system():
    """Wire the same collaborators `main.py` wires, real end to end."""
    id_generator = UuidIdGenerator()
    event_bus = InMemoryEventBus()
    book_repository = InMemoryBookRepository()
    library_repository = InMemoryLibraryRepository()

    library_id = LibraryId(id_generator.new_id())
    library_repository.save(Library(library_id, name="Test Library"))

    event_bus.subscribe(
        BookRegistered,
        AddBookToCirculationHandler(library_repository, library_id),
    )

    return {
        "library_id": library_id,
        "library_repository": library_repository,
        "register_book": RegisterBookService(book_repository, event_bus, id_generator),
        "retire_book": RetireBookService(book_repository, event_bus),
        "borrow_book": BorrowBookService(library_repository, event_bus, id_generator),
        "return_book": ReturnBookService(library_repository, event_bus, id_generator),
        "reserve_book": ReserveBookService(library_repository, event_bus, id_generator),
        "check_overdue": CheckOverdueLoansService(library_repository, event_bus),
    }


class TestRegisterBookService:
    def test_registering_a_book_adds_it_to_borrowing_circulation(self, system) -> None:
        # This is the cross-context flow test: a Catalog use case, with no
        # direct call into Borrowing, still results in Borrowing state
        # changing - purely through the BookRegistered domain event.
        system["register_book"].register_book(ISBN, "Domain-Driven Design", "Eric Evans", copies=2)

        library = system["library_repository"].get_by_id(system["library_id"])
        from borrowing.domain.value_objects import ISBN as BorrowingISBN

        assert library.available_copies(BorrowingISBN(ISBN)) == 2


class TestBorrowReturnReserveFlow:
    def test_full_borrow_reserve_return_cycle(self, system) -> None:
        library_id = system["library_id"]
        system["register_book"].register_book(ISBN, "Domain-Driven Design", "Eric Evans", copies=1)

        alice = MemberId(uuid4())
        bob = MemberId(uuid4())

        loan = system["borrow_book"].borrow_book(library_id, ISBN, alice)

        with pytest.raises(NoAvailableCopiesError):
            system["borrow_book"].borrow_book(library_id, ISBN, bob)

        reservation = system["reserve_book"].reserve_book(library_id, ISBN, bob)
        assert reservation.fulfilled is False

        system["return_book"].return_book(library_id, loan.id)

        library = system["library_repository"].get_by_id(library_id)
        fulfilled = [r for r in library.reservations if r.id == reservation.id][0]
        assert fulfilled.fulfilled is True
        # Bob's fulfilled reservation should have produced a real Loan for him.
        assert any(l.member_id == bob for l in library.loans)


class TestCheckOverdueLoansService:
    def test_detects_overdue_loan(self, system) -> None:
        library_id = system["library_id"]
        system["register_book"].register_book(ISBN, "Domain-Driven Design", "Eric Evans", copies=1)

        member = MemberId(uuid4())
        long_ago = date.today() - timedelta(days=30)
        system["borrow_book"].borrow_book(library_id, ISBN, member, today=long_ago)

        overdue = system["check_overdue"].check_overdue_loans(library_id)

        assert len(overdue) == 1


class TestRetireBookService:
    def test_retire_transitions_book_status(self, system) -> None:
        book = system["register_book"].register_book(
            ISBN, "Domain-Driven Design", "Eric Evans", copies=1
        )

        retired = system["retire_book"].retire_book(book.id)

        assert retired.status is BookStatus.RETIRED
