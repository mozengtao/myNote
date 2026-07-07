"""
Borrowing / Domain / Entities

DDD Concept: Aggregate boundary
-----------------------------------
`Library` is the Aggregate Root for this whole module. `Loan` and
`Reservation` are Entities that exist ONLY inside a `Library` - there is
no `LoanRepository` or `ReservationRepository` anywhere in this project,
and nothing outside this module ever constructs a `Loan` directly. That
is the aggregate boundary: external code (Application Services) may only
ever say "Library, please borrow/return/reserve a book", and the
`Library` decides internally how its `Loan`/`Reservation`/`LoanableBook`
objects change. This is what makes the invariants below IMPOSSIBLE to
bypass - there is no back door that mutates a `Loan`'s copy count without
going through `Library`.

`LoanableBook` (circulation record per ISBN) is deliberately NOT an
`Entity` subclass - it has no identity that matters beyond "the
circulation record for this ISBN", so a plain class keyed by ISBN in a
dict is enough; wrapping it as an Entity would add ceremony without
teaching anything new.
"""

from __future__ import annotations

from datetime import date
from enum import Enum, auto

from borrowing.domain.events import (
    BookAddedToCirculation,
    BookBorrowed,
    BookOverdue,
    BookReturned,
    ReservationCreated,
    ReservationFulfilled,
)
from borrowing.domain.value_objects import (
    ISBN,
    LibraryId,
    LoanId,
    LoanPeriod,
    MemberId,
    ReservationId,
)
from shared.entity import AggregateRoot, Entity


class BookNotInCirculationError(Exception):
    """Raised when an ISBN unknown to this Library is borrowed/reserved."""


class NoAvailableCopiesError(Exception):
    """Raised when trying to borrow a title with zero available copies."""


class CopiesAvailableError(Exception):
    """Raised when trying to reserve a title that currently has a free copy
    (members should borrow it instead of queuing for it)."""


class LoanNotFoundError(Exception):
    """Raised when returning a loan id the Library has no record of."""


class LoanAlreadyReturnedError(Exception):
    """Raised when returning a loan that was already marked returned."""


class LoanStatus(Enum):
    ACTIVE = auto()
    RETURNED = auto()


class Loan(Entity[LoanId]):
    """
    A single member's borrowing of a single copy of a title.

    Why this is an Entity
    -------------------------
    Two loans can have identical isbn/member/dates (e.g. the same member
    borrows, returns, then borrows the exact same title again) yet must
    never be confused with each other - each `Loan` has its own identity
    and its own lifecycle (active -> returned).
    """

    def __init__(
        self,
        loan_id: LoanId,
        isbn: ISBN,
        member_id: MemberId,
        period: LoanPeriod,
        status: LoanStatus = LoanStatus.ACTIVE,
    ) -> None:
        super().__init__(loan_id)
        self.isbn = isbn
        self.member_id = member_id
        self.period = period
        self.status = status

    def mark_returned(self) -> None:
        """
        Transition this loan to RETURNED.

        Why the invariant lives here, not in `Library.return_book`
        ----------------------------------------------------------------
        "You cannot return an already-returned loan" concerns only this
        loan's own two-state lifecycle. `Library` still enforces the
        Aggregate-wide invariants (e.g. which loan/reservation gets the
        freed copy) - see `Library.return_book`.
        """
        if self.status is LoanStatus.RETURNED:
            raise LoanAlreadyReturnedError(f"Loan {self._id} was already returned")
        self.status = LoanStatus.RETURNED

    def is_overdue(self, as_of: date) -> bool:
        return self.status is LoanStatus.ACTIVE and self.period.is_overdue(as_of)


class Reservation(Entity[ReservationId]):
    """A member waiting in line (FIFO) for a copy of a specific title."""

    def __init__(
        self,
        reservation_id: ReservationId,
        isbn: ISBN,
        member_id: MemberId,
        created_on: date,
        fulfilled: bool = False,
    ) -> None:
        super().__init__(reservation_id)
        self.isbn = isbn
        self.member_id = member_id
        self.created_on = created_on
        self.fulfilled = fulfilled

    def mark_fulfilled(self) -> None:
        self.fulfilled = True


class LoanableBook:
    """
    Internal circulation record: how many copies of one ISBN this Library
    owns, and how many are currently free to lend.

    Why this is NOT its own Aggregate/Entity
    ---------------------------------------------
    It has no lifecycle or identity that matters independently of the
    `Library` that owns it - "5 copies of ISBN X, 2 available" only means
    something in the context of a specific `Library`'s circulation. It is
    intentionally a plain, mutable helper object, private to this module.
    """

    def __init__(self, isbn: ISBN, total_copies: int) -> None:
        self.isbn = isbn
        self.total_copies = total_copies
        self.available_copies = total_copies

    def add_copies(self, count: int) -> None:
        self.total_copies += count
        self.available_copies += count

    def checkout(self) -> None:
        if self.available_copies <= 0:
            raise NoAvailableCopiesError(f"No available copies of {self.isbn}")
        self.available_copies -= 1

    def checkin(self) -> None:
        self.available_copies = min(self.total_copies, self.available_copies + 1)


class Library(AggregateRoot[LibraryId]):
    """
    Aggregate Root of the Borrowing context.

        Library
            |
            +-- LoanableBook  (per ISBN: total/available copy counts)
            +-- Loan          (a member currently/previously holding a copy)
            +-- Reservation   (a member queued for a copy)

    Why `Library` is the aggregate boundary
    --------------------------------------------
    Borrowing/returning/reserving all need to agree, AT THE SAME TIME, on
    "how many copies of this ISBN are free right now" - that is exactly
    the kind of invariant an Aggregate exists to protect. If `Loan` and
    `LoanableBook` were separately-saved aggregates, two concurrent
    "borrow" operations could both see "1 copy available" and both
    succeed, over-lending the title. By making `Library` the single
    consistency boundary, `LibraryRepository.save()` persists the whole
    cluster atomically and this race cannot happen (in this in-memory demo,
    the "atomicity" is trivial, but the *modeling* discipline is the point -
    it is what would let a real system add a database transaction or an
    optimistic-concurrency version check at exactly this seam).
    """

    def __init__(self, library_id: LibraryId, name: str) -> None:
        super().__init__(library_id)
        self.name = name
        self._circulation: dict[str, LoanableBook] = {}
        self._loans: dict[LoanId, Loan] = {}
        # A plain list preserves insertion order, which is exactly what we
        # need to serve reservations FIFO (first come, first served).
        self._reservations: list[Reservation] = []

    @property
    def loans(self) -> list[Loan]:
        return list(self._loans.values())

    @property
    def reservations(self) -> list[Reservation]:
        return list(self._reservations)

    def available_copies(self, isbn: ISBN) -> int:
        record = self._circulation.get(isbn.value)
        return record.available_copies if record else 0

    def add_to_circulation(self, isbn: ISBN, copies: int) -> None:
        """
        Make `copies` physical copies of `isbn` available for lending.

        Why this method exists
        ---------------------------
        This is the landing point for the cross-context integration: the
        `AddBookToCirculationHandler` (Infrastructure) calls this in
        reaction to Catalog's `BookRegistered` event. `Library` never
        imports anything from `catalog/` - it only receives an `ISBN`
        value and a count, which is all it needs to do its job.
        """
        record = self._circulation.get(isbn.value)
        if record is None:
            self._circulation[isbn.value] = LoanableBook(isbn, copies)
        else:
            record.add_copies(copies)
        self._record_event(
            BookAddedToCirculation(library_id=self._id, isbn=isbn, copies_added=copies)
        )

    def borrow_book(
        self, isbn: ISBN, member_id: MemberId, loan_id: LoanId, today: date
    ) -> Loan:
        """
        Check out one copy of `isbn` to `member_id`.

        Invariant enforced here
        ----------------------------
        Borrowing is only possible while `available_copies > 0`. This is
        checked and decremented in the SAME method, on the SAME aggregate
        instance, so there is no window where two callers could both read
        "1 available" and both succeed - see the class docstring on why
        that matters.
        """
        record = self._circulation.get(isbn.value)
        if record is None:
            raise BookNotInCirculationError(f"{isbn} is not in circulation here")

        record.checkout()  # raises NoAvailableCopiesError if none are free
        period = LoanPeriod.starting(today)
        loan = Loan(loan_id=loan_id, isbn=isbn, member_id=member_id, period=period)
        self._loans[loan_id] = loan

        self._record_event(
            BookBorrowed(
                library_id=self._id,
                loan_id=loan_id,
                isbn=isbn,
                member_id=member_id,
                due_on=period.due_on,
            )
        )
        return loan

    def return_book(
        self, loan_id: LoanId, today: date, next_loan_id: LoanId | None = None
    ) -> Loan:
        """
        Hand back a previously-borrowed copy, then FIFO-fulfill the oldest
        pending reservation for that title, if any, by immediately
        issuing a brand-new `Loan` to the waiting member.

        Why reservation fulfillment happens HERE
        ---------------------------------------------
        "The next person in line gets the copy before it goes back into
        the general pool" is a cross-cutting rule between `Loan` and
        `Reservation` - neither entity alone has enough context to enforce
        it. That makes it the Aggregate Root's job, not either child
        entity's.

        Why `next_loan_id` is an optional parameter supplied by the caller
        -----------------------------------------------------------------
        Whether a reservation actually gets fulfilled is a decision only
        this aggregate can make (it depends on aggregate-internal state),
        but Entity identity must still be generated by an `IdGenerator`
        port, which the Domain layer does not depend on directly (see
        `shared/id_generator.py`). The `ReturnBookService` Application
        Service pre-generates a spare id and passes it in "just in case" -
        it is simply discarded if no reservation is waiting.
        """
        loan = self._loans.get(loan_id)
        if loan is None:
            raise LoanNotFoundError(f"No loan with id {loan_id}")

        loan.mark_returned()
        record = self._circulation[loan.isbn.value]
        record.checkin()

        self._record_event(
            BookReturned(
                library_id=self._id,
                loan_id=loan_id,
                isbn=loan.isbn,
                member_id=loan.member_id,
            )
        )

        self._fulfill_next_reservation(loan.isbn, today, next_loan_id)
        return loan

    def _fulfill_next_reservation(
        self, isbn: ISBN, today: date, next_loan_id: LoanId | None
    ) -> None:
        """Grant the freed copy to the longest-waiting reservation, if any,
        by issuing them a new Loan for it."""
        if next_loan_id is None:
            return  # no spare id was supplied; leave any reservation pending

        for reservation in self._reservations:
            if reservation.isbn == isbn and not reservation.fulfilled:
                record = self._circulation[isbn.value]
                record.checkout()  # the copy is claimed immediately by the reservation
                reservation.mark_fulfilled()

                period = LoanPeriod.starting(today)
                new_loan = Loan(
                    loan_id=next_loan_id,
                    isbn=isbn,
                    member_id=reservation.member_id,
                    period=period,
                )
                self._loans[next_loan_id] = new_loan

                self._record_event(
                    ReservationFulfilled(
                        library_id=self._id,
                        reservation_id=reservation.id,
                        isbn=isbn,
                        member_id=reservation.member_id,
                    )
                )
                self._record_event(
                    BookBorrowed(
                        library_id=self._id,
                        loan_id=next_loan_id,
                        isbn=isbn,
                        member_id=reservation.member_id,
                        due_on=period.due_on,
                    )
                )
                return

    def reserve_book(
        self, isbn: ISBN, member_id: MemberId, reservation_id: ReservationId, today: date
    ) -> Reservation:
        """
        Queue `member_id` for the next available copy of `isbn`.

        Invariant enforced here
        ----------------------------
        A reservation only makes sense when there is currently nothing to
        borrow - if a copy IS available, the member should call
        `borrow_book` instead. Raising here prevents "phantom" reservations
        that would immediately be fulfillable, keeping reservation state
        meaningful.
        """
        record = self._circulation.get(isbn.value)
        if record is None:
            raise BookNotInCirculationError(f"{isbn} is not in circulation here")
        if record.available_copies > 0:
            raise CopiesAvailableError(
                f"{isbn} currently has {record.available_copies} copies available; "
                "borrow it instead of reserving it"
            )

        reservation = Reservation(
            reservation_id=reservation_id,
            isbn=isbn,
            member_id=member_id,
            created_on=today,
        )
        self._reservations.append(reservation)
        self._record_event(
            ReservationCreated(
                library_id=self._id,
                reservation_id=reservation_id,
                isbn=isbn,
                member_id=member_id,
            )
        )
        return reservation

    def find_overdue_loans(self, as_of: date) -> list[Loan]:
        """
        Scan active loans for overdue ones and record a `BookOverdue`
        event for each (used by `CheckOverdueLoansService` - the
        Borrowing-context analogue of a periodic health check).
        """
        overdue = [loan for loan in self._loans.values() if loan.is_overdue(as_of)]
        for loan in overdue:
            self._record_event(
                BookOverdue(
                    library_id=self._id,
                    loan_id=loan.id,
                    isbn=loan.isbn,
                    member_id=loan.member_id,
                    days_overdue=loan.period.days_overdue(as_of),
                )
            )
        return overdue
