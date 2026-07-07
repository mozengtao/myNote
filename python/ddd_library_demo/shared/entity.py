"""
Entity + AggregateRoot base classes

DDD Concept: Entity
-------------------
An Entity is defined by IDENTITY, not by its attributes. A `Book` whose
title is corrected is still "the same book" - what makes it "the same" is
its `BookId`, not the current values of its fields. This is the opposite of
a Value Object, which has no identity and is defined entirely by its
attributes.

Contrast with an anemic data class: an Entity is expected to carry
BEHAVIOUR (methods that change its own state while protecting invariants),
not just fields with public setters. Concrete entities in this project
(`Book`, `Library`, `Loan`, `Reservation`) all expose intention-revealing
methods (`retire()`, `borrow_book()`, `mark_returned()`) instead of letting
callers mutate attributes directly.

DDD Concept: Aggregate Root
---------------------------
An Aggregate Root is a special Entity: it is the ONLY object external code
is allowed to hold a reference to for its whole cluster of objects (the
"aggregate"). Everything reachable only through the root (e.g. `Loan` and
`Reservation` inside `Library`) is protected by the root's invariants -
callers cannot reach in and mutate a `Loan` directly, bypassing the rules
`Library` enforces.

`AggregateRoot` extends `Entity` with the ability to *record* Domain
Events as it changes state. Recording (not publishing!) happens inside the
domain method itself - publishing is an Infrastructure/Application
concern, done only after the aggregate has been durably saved. This is why
`pull_events()` drains the list rather than the aggregate publishing
directly: the Domain layer must never depend on `EventBus`.
"""

from __future__ import annotations

from abc import ABC
from typing import Generic, TypeVar

from shared.domain_event import DomainEvent

# The identity type varies per entity (e.g. `BookId`, `LibraryId`), so the
# base class is generic over it rather than assuming e.g. `UUID`.
IdType = TypeVar("IdType")


class Entity(ABC, Generic[IdType]):
    """
    Base class for anything whose equality is defined by identity.

    Why equality is overridden
    ---------------------------
    Two `Entity` instances are equal if and only if they are the same
    concrete type AND carry the same identity - never based on their other
    attributes. This is the defining trait that separates an Entity from a
    Value Object.
    """

    def __init__(self, entity_id: IdType) -> None:
        self._id = entity_id

    @property
    def id(self) -> IdType:
        return self._id

    def __eq__(self, other: object) -> bool:
        if type(other) is not type(self):
            return NotImplemented
        return self._id == other._id

    def __hash__(self) -> int:
        return hash((type(self), self._id))


class AggregateRoot(Entity[IdType], Generic[IdType]):
    """
    Base class for Entities that act as the transactional consistency
    boundary for a cluster of objects, and that raise Domain Events.

    Why events are recorded here instead of published immediately
    ----------------------------------------------------------------
    If a domain method published events directly, the Domain layer would
    need to depend on an `EventBus` implementation - violating the
    Dependency Rule (Domain must not depend on Infrastructure). Instead,
    domain methods call `self._record_event(...)`, and the surrounding
    Application Service calls `pull_events()` AFTER the repository has
    saved the aggregate, then hands the events to the `EventBus`. This
    guarantees events are only ever published for state changes that were
    actually persisted.
    """

    def __init__(self, entity_id: IdType) -> None:
        super().__init__(entity_id)
        self._pending_events: list[DomainEvent] = []

    def _record_event(self, event: DomainEvent) -> None:
        """Called by domain methods to note "this happened", not to publish it."""
        self._pending_events.append(event)

    def pull_events(self) -> list[DomainEvent]:
        """
        Drain and return all events recorded since the last pull.

        Why "pull and clear" rather than a read-only property
        ---------------------------------------------------------
        Draining prevents the same event from being published twice if an
        Application Service accidentally calls this more than once, and
        keeps the aggregate's in-memory event list from growing unbounded
        across its lifetime.
        """
        events, self._pending_events = self._pending_events, []
        return events
