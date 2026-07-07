"""
Borrowing / Domain / Events

Each event is a past-tense fact recorded by the `Library` Aggregate Root
as a side effect of one of its business methods. `infrastructure/`
subscribes handlers to these (e.g. a statistics counter, or the FIFO
reservation-fulfillment logic that reacts to `BookReturned`).
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date

from borrowing.domain.value_objects import (
    ISBN,
    LibraryId,
    LoanId,
    MemberId,
    ReservationId,
)
from shared.domain_event import DomainEvent


@dataclass(frozen=True, kw_only=True)
class BookAddedToCirculation(DomainEvent):
    """A title became available for lending at a library (usually in
    response to Catalog's `BookRegistered`)."""

    library_id: LibraryId
    isbn: ISBN
    copies_added: int


@dataclass(frozen=True, kw_only=True)
class BookBorrowed(DomainEvent):
    """A member successfully checked out a copy of a title."""

    library_id: LibraryId
    loan_id: LoanId
    isbn: ISBN
    member_id: MemberId
    due_on: date


@dataclass(frozen=True, kw_only=True)
class BookReturned(DomainEvent):
    """A previously-borrowed copy was handed back."""

    library_id: LibraryId
    loan_id: LoanId
    isbn: ISBN
    member_id: MemberId


@dataclass(frozen=True, kw_only=True)
class ReservationCreated(DomainEvent):
    """A member was placed on the waiting list because no copies were free."""

    library_id: LibraryId
    reservation_id: ReservationId
    isbn: ISBN
    member_id: MemberId


@dataclass(frozen=True, kw_only=True)
class ReservationFulfilled(DomainEvent):
    """A waiting reservation was granted a newly-returned copy."""

    library_id: LibraryId
    reservation_id: ReservationId
    isbn: ISBN
    member_id: MemberId


@dataclass(frozen=True, kw_only=True)
class BookOverdue(DomainEvent):
    """A loan's due date has passed without the copy being returned."""

    library_id: LibraryId
    loan_id: LoanId
    isbn: ISBN
    member_id: MemberId
    days_overdue: int
