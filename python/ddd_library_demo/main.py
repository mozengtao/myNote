"""
main.py - Runnable demonstration of the whole system

This script is the CLI entry point referenced throughout the object-flow
diagrams in the README: every use case below starts here, flows through
an Application Service, into a Domain Aggregate, and (where relevant) out
through a Domain Event to a handler and back into a Repository.

Run with:

    python main.py

No external dependencies are required to run this file - only `pytest`
(for the test suite) is listed in requirements.txt.
"""

from __future__ import annotations

from datetime import date, timedelta
from uuid import uuid4

from borrowing.application.services import (
    BorrowBookService,
    CheckOverdueLoansService,
    ReserveBookService,
    ReturnBookService,
)
from borrowing.domain.entities import Library, NoAvailableCopiesError
from borrowing.domain.events import BookBorrowed, BookOverdue, BookReturned
from borrowing.domain.events import ReservationCreated, ReservationFulfilled
from borrowing.domain.value_objects import LibraryId, MemberId
from catalog.application.services import RegisterBookService, RetireBookService
from catalog.domain.events import BookRegistered
from infrastructure.event_bus import InMemoryEventBus
from infrastructure.event_handlers import AddBookToCirculationHandler, BorrowingStatsHandler
from infrastructure.id_generator import UuidIdGenerator
from infrastructure.in_memory_repositories import (
    InMemoryBookRepository,
    InMemoryLibraryRepository,
)
from infrastructure.logging_setup import configure_logging


def section(title: str) -> None:
    print()
    print(f"===== {title} =====")


def bootstrap() -> dict:
    """
    Wire up every Infrastructure adapter and cross-context event handler.

    This function is the ONLY place in the whole project that is allowed
    to know about every layer at once - it is the Composition Root.
    Domain and Application code never see these concrete classes, only
    the Protocols they implement.
    """
    configure_logging()

    id_generator = UuidIdGenerator()
    event_bus = InMemoryEventBus()

    book_repository = InMemoryBookRepository()
    library_repository = InMemoryLibraryRepository()

    # Seed a single-branch Library aggregate. In a bigger system this
    # would be its own "CreateLibraryService" use case; here, one branch
    # is enough to demonstrate the object flow without extra ceremony.
    library_id = LibraryId(id_generator.new_id())
    library_repository.save(Library(library_id=library_id, name="Central Library"))

    # Cross-context wiring: Catalog publishes BookRegistered: Borrowing
    # reacts by adding the title to circulation. Neither context's
    # domain/application code knows this wiring exists.
    circulation_handler = AddBookToCirculationHandler(library_repository, library_id)
    event_bus.subscribe(BookRegistered, circulation_handler)

    stats = BorrowingStatsHandler()
    event_bus.subscribe(BookBorrowed, stats.on_book_borrowed)
    event_bus.subscribe(BookReturned, stats.on_book_returned)
    event_bus.subscribe(ReservationCreated, stats.on_reservation_created)
    event_bus.subscribe(ReservationFulfilled, stats.on_reservation_fulfilled)
    event_bus.subscribe(BookOverdue, stats.on_book_overdue)

    return {
        "library_id": library_id,
        "id_generator": id_generator,
        "event_bus": event_bus,
        "book_repository": book_repository,
        "library_repository": library_repository,
        "stats": stats,
        "register_book": RegisterBookService(book_repository, event_bus, id_generator),
        "retire_book": RetireBookService(book_repository, event_bus),
        "borrow_book": BorrowBookService(library_repository, event_bus, id_generator),
        "return_book": ReturnBookService(library_repository, event_bus, id_generator),
        "reserve_book": ReserveBookService(library_repository, event_bus, id_generator),
        "check_overdue": CheckOverdueLoansService(library_repository, event_bus),
    }


def main() -> None:
    ctx = bootstrap()
    library_id: LibraryId = ctx["library_id"]

    alice = MemberId(uuid4())
    bob = MemberId(uuid4())
    carol = MemberId(uuid4())

    ddd_isbn = "9780321125217"  # Domain-Driven Design, Eric Evans
    clean_arch_isbn = "9780134494166"  # Clean Architecture, Robert C. Martin

    section("1. Register books in the Catalog (cross-context flow to Borrowing)")
    ddd_book = ctx["register_book"].register_book(
        isbn=ddd_isbn, title="Domain-Driven Design", author="Eric Evans", copies=1
    )
    clean_arch_book = ctx["register_book"].register_book(
        isbn=clean_arch_isbn, title="Clean Architecture", author="Robert C. Martin", copies=2
    )

    section("2. Alice borrows the only copy of Domain-Driven Design")
    alice_loan = ctx["borrow_book"].borrow_book(library_id, ddd_isbn, alice)

    section("3. Bob tries to borrow the same title -> no copies -> Bob reserves it instead")
    try:
        ctx["borrow_book"].borrow_book(library_id, ddd_isbn, bob)
    except NoAvailableCopiesError as exc:
        print(f"[Demo] Borrow failed as expected: {exc}")
        ctx["reserve_book"].reserve_book(library_id, ddd_isbn, bob)

    section("4. Alice returns her copy -> Bob's reservation is fulfilled FIFO")
    ctx["return_book"].return_book(library_id, alice_loan.id)

    section("5. Carol borrows Clean Architecture 20 days ago -> now it is overdue")
    twenty_days_ago = date.today() - timedelta(days=20)
    ctx["borrow_book"].borrow_book(library_id, clean_arch_isbn, carol, today=twenty_days_ago)
    ctx["check_overdue"].check_overdue_loans(library_id)

    section("6. Retire Domain-Driven Design from the catalog")
    ctx["retire_book"].retire_book(ddd_book.id)

    section("7. Final state")
    library = ctx["library_repository"].get_by_id(library_id)
    print(f"Library: {library.name} ({library.id})")
    print(f"  Active loans: {len(library.loans)}")
    print(f"  Reservations: {len(library.reservations)}")
    print(f"  Catalog status of '{ddd_book.title}': {ddd_book.status.name}")
    print(f"  Catalog status of '{clean_arch_book.title}': {clean_arch_book.status.name}")
    print(f"  Stats counters: {dict(ctx['stats'].counters)}")
    print()
    print("Done.")


if __name__ == "__main__":
    main()
