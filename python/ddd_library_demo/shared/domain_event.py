"""
Domain Event base + EventBus interface

DDD Concept: Domain Event
-------------------------
A Domain Event is a record of something that HAPPENED in the domain that
other parts of the system (inside or outside the aggregate that raised it)
may care about. Events are named in the past tense - `BookBorrowed`, not
`BorrowBook` - because they describe a fact, not a command.

Events are how this project achieves two things at once:
1. Decoupling inside a bounded context (an Aggregate does not call other
   aggregates directly; it raises an event and lets a handler react).
2. Decoupling BETWEEN bounded contexts (Catalog never imports Borrowing's
   domain model; it publishes `BookRegistered` and Borrowing subscribes).

Why the EventBus interface (a `Protocol`) lives in the Shared Kernel
---------------------------------------------------------------------
Application Services need to publish events, and Domain code needs a
vocabulary for "a thing that can happen". But the *mechanism* used to
actually deliver events (in-memory, a message queue, ...) is an
Infrastructure concern. Defining the `EventBus` Protocol here - not in
`infrastructure/` - lets Domain/Application code depend only on this
interface (Dependency Inversion), while `infrastructure/event_bus.py`
provides the concrete in-memory implementation. Domain and Application
layers never import `infrastructure/`.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Callable, Protocol


@dataclass(frozen=True, kw_only=True)
class DomainEvent:
    """
    Base class for every Domain Event in the system.

    Why it exists
    -------------
    Gives every concrete event (BookRegistered, BookBorrowed, ...) a common
    shape - in particular `occurred_at`, so any handler or log line can
    answer "when did this happen?" without each event redefining it.

    Why it is a frozen dataclass
    ----------------------------
    Events describe something that already happened. History does not
    change, so an event, once created, must be immutable - exactly the
    same reasoning that makes Value Objects immutable.
    """

    occurred_at: datetime = field(
        default_factory=lambda: datetime.now(timezone.utc)
    )


# A handler is just "something callable with one event argument". Using a
# plain `Callable` (rather than an abstract base class with a `handle()`
# method) keeps handler registration lightweight - any function, method, or
# lambda can subscribe.
EventHandler = Callable[[DomainEvent], None]


class EventBus(Protocol):
    """
    Interface (Protocol) describing "something that can publish and
    dispatch Domain Events".

    Why it is a `Protocol`, not an ABC
    ------------------------------------
    `typing.Protocol` gives structural typing: `infrastructure.InMemoryEventBus`
    does not need to inherit from this class, it merely needs to implement
    `publish` and `subscribe` with matching signatures. This keeps the
    Domain/Application layers decoupled even from the *inheritance
    hierarchy* of Infrastructure, not just its data.

    Why it belongs in the Shared Kernel
    ------------------------------------
    Both bounded contexts' Application Services publish events through this
    exact interface, and Infrastructure event handlers subscribe through
    it. It is the seam where Dependency Inversion happens for the eventing
    mechanism, mirroring how each context's `repository.py` is the seam for
    persistence.
    """

    def publish(self, event: DomainEvent) -> None:
        """Deliver `event` synchronously to every subscriber of its type."""
        ...

    def subscribe(self, event_type: type[DomainEvent], handler: EventHandler) -> None:
        """Register `handler` to be invoked whenever `event_type` is published."""
        ...
