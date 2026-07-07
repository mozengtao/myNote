# DDD Library Demo

A small, heavily-commented, **runnable** Python project whose sole purpose is
to teach the core concepts of **Domain-Driven Design (DDD)** through a
realistic-but-tiny **Library Management / Book Borrowing** system.

This is not a production system. There is no database, no web framework, no
message broker. Every "Infrastructure" component is an in-memory stand-in,
on purpose - so that every design decision that matters (aggregates,
invariants, bounded contexts, events) stays visible instead of being buried
under framework code.

Assumed audience: you already know Python; you are new to (or want to
solidify) DDD.

## Project Purpose

Every class, and every important method, carries a docstring answering
three questions:

1. **Why does this exist?**
2. **What is its responsibility?**
3. **Which DDD concept does it demonstrate?**

Read the code itself, not just this README - the comments are the primary
teaching material.

## How to Run

```bash
cd python/ddd_library_demo

# Run the narrated demonstration script (no dependencies required):
python main.py

# Run the test suite (requires pytest, see requirements.txt):
pip install -r requirements.txt
pytest
```

No `pip install -e .` / packaging step is required to run either command;
`pyproject.toml` sets `pythonpath = ["."]` for pytest, and `main.py` is a
plain script.

### Expected Output (excerpt)

```
===== 1. Register books in the Catalog (cross-context flow to Borrowing) =====
[Catalog] Registering book 'Domain-Driven Design' (9780321125217)...
[EventHandler] BookRegistered received for 9780321125217 -> adding to circulation...
[Catalog] Book registered and BookRegistered event published.
...
===== 3. Bob tries to borrow the same title -> no copies -> Bob reserves it instead =====
[Borrowing] ... is borrowing 9780321125217...
[Demo] Borrow failed as expected: No available copies of 9780321125217
[Borrowing] ... is reserving 9780321125217...
[Stats] reservations_created=1
[Borrowing] Reservation created; ReservationCreated published.
...
Done.
```

## Architecture Overview

The project uses a **Layered / Hexagonal Architecture**, split into **two
Bounded Contexts** that share a small **Shared Kernel** and a common
**Infrastructure** layer:

```
python/ddd_library_demo/
├── main.py                # Composition Root: wires everything together, runs the demo
├── shared/                # Shared Kernel: Entity/AggregateRoot, ValueObject, DomainEvent/EventBus, IdGenerator
├── catalog/                # Bounded Context 1: "what books exist"
│   ├── domain/             #   entities, value objects, events, repository interface
│   └── application/        #   use-case orchestration
├── borrowing/               # Bounded Context 2: "who has which copy"
│   ├── domain/              #   entities, value objects, events, repository interface, domain service
│   └── application/         #   use-case orchestration
├── infrastructure/          # Replaceable adapters: in-memory repos, event bus, event handlers, id gen, logging
└── tests/                   # pytest suite (no mocks - real in-memory infra)
```

### Dependency Direction

```
        Application
             |
             v
          Domain          <-- Domain depends on NOTHING project-specific
             ^
             |
      Infrastructure
```

`catalog/domain/` and `borrowing/domain/` never import from
`infrastructure/`, from each other, or from their own `application/`
package. Every module that would break this rule says so explicitly in its
docstring. This is the **Dependency Rule**: dependencies always point
*inward*, toward the Domain.

## Bounded Contexts

```
+------------------------------+        BookRegistered         +-----------------------------+
|      Catalog Context         | ----------------------------> |     Borrowing Context       |
|  "what books exist"          |         (Domain Event)        |  "who has which copy"       |
|                              |                               |                             |
|  Aggregate Root: Book        |                               |  Aggregate Root: Library    |
|  - title, author, isbn       |                               |  - LoanableBook (per ISBN)  |
|  - lifecycle: active/retired |                               |  - Loan, Reservation        |
+------------------------------+                               +-----------------------------+
```

The two contexts never call each other's domain code directly. The **only**
integration point is `infrastructure/event_handlers.py`, where
`AddBookToCirculationHandler` subscribes to Catalog's `BookRegistered` event
and reacts by calling `Library.add_to_circulation(...)`. This demonstrates
Domain Events as the mechanism for **Bounded Context integration**, not just
in-context decoupling.

Note that each context defines its **own** `ISBN` Value Object
(`catalog/domain/value_objects.py` vs `borrowing/domain/value_objects.py`).
This is deliberate: Catalog needs full ISBN-10/13 checksum validation
(bibliographic accuracy is its job); Borrowing only needs a stable, opaque
key. Sharing one `ISBN` class across contexts would create a hidden coupling
between them - see the module docstring in
`borrowing/domain/value_objects.py` for the full reasoning.

## DDD Concepts, and Where to Find Them

| Concept | Where | Notes |
|---|---|---|
| **Entity** | `catalog/domain/entities.py::Book`, `borrowing/domain/entities.py::Loan`, `Reservation` | Identity-based equality (`shared/entity.py::Entity.__eq__`), behaviour instead of public setters. |
| **Value Object** | `catalog/domain/value_objects.py` (`ISBN`, `BookTitle`, `AuthorName`, `BookId`), `borrowing/domain/value_objects.py` (`MemberId`, `LoanId`, `ReservationId`, `LoanPeriod`) | Immutable (`@dataclass(frozen=True)`), compare-by-value, self-validating in `__post_init__`. |
| **Aggregate Root** | `catalog/domain/entities.py::Book`, `borrowing/domain/entities.py::Library` | `Library` is the more interesting one: it protects `LoanableBook`/`Loan`/`Reservation` as a single consistency boundary. See its class docstring for why. |
| **Domain Event** | `catalog/domain/events.py`, `borrowing/domain/events.py` | Frozen dataclasses inheriting `shared/domain_event.py::DomainEvent`. Recorded via `AggregateRoot._record_event`, drained via `pull_events()`. |
| **Application Service** | `catalog/application/services.py`, `borrowing/application/services.py` | Orchestration only: load aggregate -> call domain method -> save -> publish events. No business rules live here. |
| **Domain Service** | `borrowing/domain/services.py::LateFeePolicy` | Computes a late fee - deliberately NOT a `Loan` method; see its docstring for why a fee schedule is a policy, not an intrinsic property of a loan. |
| **Repository (interface)** | `catalog/domain/repository.py`, `borrowing/domain/repository.py` | `Protocol`s defined in the Domain layer - Dependency Inversion for persistence. |
| **Repository (implementation)** | `infrastructure/in_memory_repositories.py` | Plain `dict`-backed, no database. |
| **Bounded Context** | `catalog/`, `borrowing/` (top-level packages) | See the "Bounded Contexts" section above. |
| **Shared Kernel** | `shared/` | Deliberately tiny: `Entity`/`AggregateRoot`, `ValueObject`, `DomainEvent`/`EventBus`, `IdGenerator`. |

## Aggregate Diagram: `Library`

```
Library (Aggregate Root)
    |
    +-- LoanableBook   (per ISBN: total_copies / available_copies)
    +-- Loan           (member_id, isbn, LoanPeriod, status)
    +-- Reservation    (member_id, isbn, created_on, fulfilled)
```

Invariants enforced by `Library` (never bypassable from outside):

- A book can only be borrowed while `available_copies > 0`.
- A book can only be reserved while `available_copies == 0` (otherwise,
  borrow it instead).
- A `Loan` can be returned at most once.
- When a copy is returned, the **oldest** pending reservation for that ISBN
  (FIFO) is fulfilled first, before the copy goes back into the general
  pool - fulfillment immediately issues a new `Loan` to the waiting member.

## Object Flow Diagrams

### Flow 1 - Registering a Book (cross-context integration)

```
main.py
  |
  v
RegisterBookService            (Catalog / Application)
  |
  v
Book.register()                (Catalog / Domain, Aggregate Root - records BookRegistered)
  |
  v
BookRepository.save()          (Infrastructure)
  |
  v
EventBus.publish(BookRegistered)
  |
  v
AddBookToCirculationHandler    (Infrastructure - subscribed handler)
  |
  v
Library.add_to_circulation()   (Borrowing / Domain, Aggregate Root)
  |
  v
LibraryRepository.save()       (Infrastructure)
```

### Flow 2 - Borrowing a Book

```
main.py
  |
  v
BorrowBookService               (Borrowing / Application)
  |
  v
LibraryRepository.get_by_id()
  |
  v
Library.borrow_book()           (Aggregate Root - enforces "available_copies > 0")
  |
  v
new Loan entity created inside the aggregate
  |
  v
Library records BookBorrowed domain event
  |
  v
LibraryRepository.save()
  |
  v
EventBus.publish(BookBorrowed) -> BorrowingStatsHandler updates counters
```

### Flow 3 - Returning a Book (with FIFO reservation fulfillment)

```
main.py
  |
  v
ReturnBookService                (Borrowing / Application)
  |
  v
LibraryRepository.get_by_id()
  |
  v
Library.return_book()            (Aggregate Root)
  |         |
  |         +--> Loan.mark_returned()
  |         +--> LoanableBook.checkin()          (copy freed)
  |         +--> record BookReturned event
  |         +--> _fulfill_next_reservation()
  |                 +--> LoanableBook.checkout()  (copy re-claimed for reservation)
  |                 +--> Reservation.mark_fulfilled()
  |                 +--> new Loan created for the waiting member
  |                 +--> record ReservationFulfilled + BookBorrowed events
  v
LibraryRepository.save()
  |
  v
EventBus.publish(BookReturned, ReservationFulfilled, BookBorrowed)
        -> BorrowingStatsHandler updates counters
```

### Flow 4 - Checking for Overdue Loans (periodic "health check" style)

```
main.py
  |
  v
CheckOverdueLoansService        (Borrowing / Application)
  |
  v
Library.find_overdue_loans()    (Aggregate Root - scans active loans, records BookOverdue per hit)
  |
  v
LibraryRepository.save()
  |
  v
EventBus.publish(BookOverdue) -> BorrowingStatsHandler logs an alert
```

## Package Dependency Diagram

```
catalog/application  ---->  catalog/domain  <----  infrastructure
                                   |                     ^
                                   v                     |
                               shared/  <-----------------
                                   ^                     |
                                   |                     |
borrowing/application  -->  borrowing/domain  <----------
```

`infrastructure/` is the only package allowed to import from both
`catalog/domain` and `borrowing/domain` at once (in
`infrastructure/event_handlers.py` and
`infrastructure/in_memory_repositories.py`) - that is exactly where
cross-context wiring belongs, and nowhere else.

## Demonstration Script Walkthrough (`main.py`)

1. **Bootstrap** - `main.bootstrap()` is the project's *Composition Root*:
   the one place allowed to see every layer at once. It wires the in-memory
   repositories, the event bus, and subscribes
   `AddBookToCirculationHandler` (cross-context) and
   `BorrowingStatsHandler` (statistics).
2. **Register two books** in the Catalog - watch the console show the
   `BookRegistered` event flowing into Borrowing automatically.
3. **Alice borrows** the only copy of *Domain-Driven Design*.
4. **Bob tries to borrow** the same title, fails (`NoAvailableCopiesError`),
   and reserves it instead.
5. **Alice returns** her copy - Bob's reservation is fulfilled FIFO, and he
   is automatically issued a new loan.
6. **Carol borrows** *Clean Architecture* with a backdated loan date, then
   `CheckOverdueLoansService` finds and flags it as overdue.
7. **Domain-Driven Design is retired** from the catalog.
8. **Final state** is printed: active loans, reservations, catalog
   statuses, and the accumulated statistics counters.

## Testing Philosophy

`tests/` uses real in-memory repositories and a real in-memory event bus
throughout - no mocking libraries, per the project's educational goals.
Testing against the real (if simplified) infrastructure keeps the tests
honest about the actual object flow, rather than asserting on mocked call
arguments.

- `test_value_objects.py` - immutability, value equality, self-validation.
- `test_catalog_domain.py` - `Book` identity and lifecycle invariants.
- `test_borrowing_domain.py` - `Library` aggregate invariants, including
  the FIFO reservation-fulfillment rule.
- `test_application_services.py` - full use-case flows, including the
  cross-context Catalog -> Borrowing integration.
- `test_events.py` - event bus publish/subscribe semantics and event
  immutability.

## Code Style Notes

- Python 3.10+ syntax is used throughout (`X | None` unions, `Protocol`,
  `kw_only=True` dataclasses); the project is written in the spirit of
  Python 3.12 (no legacy `typing.Optional`/`typing.List`, etc.) while
  remaining runnable on 3.10+.
- Standard library only: `dataclasses`, `typing`, `enum`, `abc`, `uuid`,
  `logging`. No web framework, no ORM, no message broker.
- Every module states its own dependency-direction rule in its docstring,
  so you can verify the architecture by reading, not just by trusting this
  README.

## Further Reading (from this vault)

- [python_note.md](../../python_note.md) - broader personal notes/links on DDD.
- [ddd-by-examples/library](https://github.com/ddd-by-examples/library) - the
  Java reference project this demo's Catalog/Borrowing (catalogue/lending)
  split takes inspiration from.
