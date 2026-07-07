"""
Borrowing Bounded Context

Responsibility
--------------
Owns the lending truth of the library: which ISBNs are currently in
circulation, how many copies are available, who has borrowed what, and
who is waiting for a copy to free up. The Borrowing context deliberately
knows almost nothing about a book's bibliographic details - it only ever
refers to titles by their `ISBN` (a Value Object shared in spirit, though
each context defines its OWN `ISBN` type - see the note in
`borrowing/domain/value_objects.py`).

DDD Concept: Bounded Context boundary in practice
-----------------------------------------------------
Borrowing finds out about new titles only via the `BookRegistered` Domain
Event published by the Catalog context (handled in
`infrastructure/event_handlers.py`). There is no direct method call, no
shared aggregate, and no import of `catalog.domain` anywhere in this
package - the two contexts are integrated purely through events.
"""
