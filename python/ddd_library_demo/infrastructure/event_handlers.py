"""
Infrastructure / Event Handlers

DDD Concept: Bounded Context integration through Domain Events
--------------------------------------------------------------------
`AddBookToCirculationHandler` is the concrete answer to "how do Catalog
and Borrowing talk to each other without knowing about each other's
domain model?" It subscribes to Catalog's `BookRegistered` event and
reacts by calling a method on the Borrowing context's `Library`
aggregate - translating Catalog's `catalog.domain.value_objects.ISBN`
into Borrowing's own `borrowing.domain.value_objects.ISBN` at the
boundary (an "Anti-Corruption Layer"-style translation, kept intentionally
tiny here since both ISBNs are simple strings).

This module is allowed to import from BOTH `catalog.domain` and
`borrowing.domain` specifically BECAUSE it lives in `infrastructure/` -
the wiring/integration layer - not inside either context's own
`domain/` or `application/` packages, which must stay ignorant of each
other.

`BorrowingStatsHandler` demonstrates the other common reason to handle
events: maintaining a read-optimized side effect (simple counters) that
has nothing to do with the aggregate's own consistency, so it does not
belong inside `Library` itself.
"""

from __future__ import annotations

import logging
from collections import Counter

from borrowing.domain import events as borrowing_events
from borrowing.domain.repository import LibraryRepository
from borrowing.domain.value_objects import ISBN as BorrowingISBN
from borrowing.domain.value_objects import LibraryId
from catalog.domain.events import BookRegistered

logger = logging.getLogger(__name__)


class AddBookToCirculationHandler:
    """
    Reacts to Catalog's `BookRegistered` by registering the title for
    lending at a target `Library`.

    Why a target `library_id` is fixed at construction time
    -------------------------------------------------------------
    This demo project models a single-branch library, so "which Library
    should newly-cataloged titles circulate at?" has one obvious answer.
    A multi-branch system would instead look this up via a policy or
    routing rule - the event-handling PATTERN shown here would not change,
    only where the target id comes from.
    """

    def __init__(self, library_repository: LibraryRepository, library_id: LibraryId) -> None:
        self._library_repository = library_repository
        self._library_id = library_id

    def __call__(self, event: BookRegistered) -> None:
        print(
            f"[EventHandler] BookRegistered received for {event.isbn} -> "
            "adding to circulation..."
        )
        library = self._library_repository.get_by_id(self._library_id)
        # Translate Catalog's ISBN value into Borrowing's own ISBN type -
        # see the module docstring on why each context keeps its own.
        library.add_to_circulation(BorrowingISBN(str(event.isbn)), copies=event.copies)
        self._library_repository.save(library)


class BorrowingStatsHandler:
    """
    Maintains simple in-memory counters of borrowing activity, purely for
    the demo's "Statistics Updated." narration step. This is exactly the
    kind of side effect that belongs in an event handler rather than
    inside `Library`: `Library` should not need to know that anything is
    counting how often it is used.
    """

    def __init__(self) -> None:
        self.counters: Counter[str] = Counter()

    def on_book_borrowed(self, event: borrowing_events.BookBorrowed) -> None:
        self.counters["books_borrowed"] += 1
        print(f"[Stats] books_borrowed={self.counters['books_borrowed']}")

    def on_book_returned(self, event: borrowing_events.BookReturned) -> None:
        self.counters["books_returned"] += 1
        print(f"[Stats] books_returned={self.counters['books_returned']}")

    def on_reservation_created(self, event: borrowing_events.ReservationCreated) -> None:
        self.counters["reservations_created"] += 1
        print(f"[Stats] reservations_created={self.counters['reservations_created']}")

    def on_reservation_fulfilled(self, event: borrowing_events.ReservationFulfilled) -> None:
        self.counters["reservations_fulfilled"] += 1
        print(f"[Stats] reservations_fulfilled={self.counters['reservations_fulfilled']}")

    def on_book_overdue(self, event: borrowing_events.BookOverdue) -> None:
        self.counters["overdue_alerts"] += 1
        print(
            f"[Stats] overdue_alerts={self.counters['overdue_alerts']} "
            f"(loan {event.loan_id}, {event.days_overdue} day(s) overdue)"
        )
