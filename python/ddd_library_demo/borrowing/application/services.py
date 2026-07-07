"""
Borrowing / Application / Services
"""

from __future__ import annotations

import logging
from datetime import date

from borrowing.domain.entities import Loan, Reservation
from borrowing.domain.repository import LibraryRepository
from borrowing.domain.value_objects import ISBN, LibraryId, LoanId, MemberId, ReservationId
from shared.domain_event import EventBus
from shared.id_generator import IdGenerator

logger = logging.getLogger(__name__)


class BorrowBookService:
    """Use case: a member checks out a copy of a title."""

    def __init__(
        self,
        library_repository: LibraryRepository,
        event_bus: EventBus,
        id_generator: IdGenerator,
    ) -> None:
        self._library_repository = library_repository
        self._event_bus = event_bus
        self._id_generator = id_generator

    def borrow_book(
        self,
        library_id: LibraryId,
        isbn: str,
        member_id: MemberId,
        today: date | None = None,
    ) -> Loan:
        today = today or date.today()
        print(f"[Borrowing] {member_id} is borrowing {isbn}...")
        library = self._library_repository.get_by_id(library_id)

        loan = library.borrow_book(
            isbn=ISBN(isbn),
            member_id=member_id,
            loan_id=LoanId(self._id_generator.new_id()),
            today=today,
        )
        self._library_repository.save(library)

        for event in library.pull_events():
            self._event_bus.publish(event)
        print(f"[Borrowing] Loan created, due {loan.period.due_on}. BookBorrowed published.")
        return loan


class ReturnBookService:
    """Use case: a member returns a previously-borrowed copy."""

    def __init__(
        self,
        library_repository: LibraryRepository,
        event_bus: EventBus,
        id_generator: IdGenerator,
    ) -> None:
        self._library_repository = library_repository
        self._event_bus = event_bus
        self._id_generator = id_generator

    def return_book(
        self, library_id: LibraryId, loan_id: LoanId, today: date | None = None
    ) -> Loan:
        today = today or date.today()
        print(f"[Borrowing] Returning loan {loan_id}...")
        library = self._library_repository.get_by_id(library_id)

        # Pre-generate a spare LoanId in case a waiting reservation needs
        # to be turned into a brand-new Loan (see Library.return_book).
        next_loan_id = LoanId(self._id_generator.new_id())
        loan = library.return_book(loan_id, today=today, next_loan_id=next_loan_id)
        self._library_repository.save(library)

        for event in library.pull_events():
            self._event_bus.publish(event)
        print(f"[Borrowing] Loan {loan_id} returned; BookReturned published.")
        return loan


class ReserveBookService:
    """Use case: a member queues for a title that has no free copies."""

    def __init__(
        self,
        library_repository: LibraryRepository,
        event_bus: EventBus,
        id_generator: IdGenerator,
    ) -> None:
        self._library_repository = library_repository
        self._event_bus = event_bus
        self._id_generator = id_generator

    def reserve_book(
        self,
        library_id: LibraryId,
        isbn: str,
        member_id: MemberId,
        today: date | None = None,
    ) -> Reservation:
        today = today or date.today()
        print(f"[Borrowing] {member_id} is reserving {isbn}...")
        library = self._library_repository.get_by_id(library_id)

        reservation = library.reserve_book(
            isbn=ISBN(isbn),
            member_id=member_id,
            reservation_id=ReservationId(self._id_generator.new_id()),
            today=today,
        )
        self._library_repository.save(library)

        for event in library.pull_events():
            self._event_bus.publish(event)
        print(f"[Borrowing] Reservation created; ReservationCreated published.")
        return reservation


class CheckOverdueLoansService:
    """
    Use case: scan a library's active loans and flag overdue ones.

    Why this plays the role of a "health check" service
    ---------------------------------------------------------
    This is the Borrowing-context analogue of the spec's device-domain
    `HealthCheckService` example: a periodic, read-mostly operation that
    inspects aggregate state and raises events for anomalies (here,
    `BookOverdue`) rather than being triggered by a single user action.
    """

    def __init__(self, library_repository: LibraryRepository, event_bus: EventBus) -> None:
        self._library_repository = library_repository
        self._event_bus = event_bus

    def check_overdue_loans(self, library_id: LibraryId, today: date | None = None) -> list[Loan]:
        today = today or date.today()
        print(f"[Borrowing] Checking for overdue loans as of {today}...")
        library = self._library_repository.get_by_id(library_id)
        overdue_loans = library.find_overdue_loans(today)
        self._library_repository.save(library)

        for event in library.pull_events():
            self._event_bus.publish(event)

        if overdue_loans:
            print(f"[Borrowing] Found {len(overdue_loans)} overdue loan(s).")
        else:
            print("[Borrowing] No overdue loans found.")
        return overdue_loans
