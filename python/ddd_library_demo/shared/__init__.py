"""
Shared Kernel

Responsibility
--------------
Holds the small set of base building blocks that BOTH bounded contexts
(Catalog and Borrowing) agree to share: Entity/AggregateRoot identity
plumbing, a ValueObject marker, and the DomainEvent/EventBus vocabulary.

DDD Concept: Shared Kernel
--------------------------
In Domain-Driven Design, a "Shared Kernel" is a deliberately small piece of
model that two bounded contexts both depend on, because duplicating it would
cost more than the coupling it introduces. It must stay tiny and stable -
if it grows, that is a signal the contexts should own their own copies
instead.

Nothing in this package is specific to books, loans, or devices. It is pure
plumbing: it must never import from `catalog/`, `borrowing/`, or
`infrastructure/`.
"""
