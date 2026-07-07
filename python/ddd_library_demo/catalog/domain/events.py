"""
Catalog / Domain / Events

These are the only two facts about the Catalog context that the outside
world (in particular, the Borrowing bounded context) is allowed to know
about. Everything else about `Book` (its title, author, internal status
transitions) stays private to this context.

DDD Concept: Domain Event as a Bounded-Context contract
----------------------------------------------------------
`BookRegistered` and `BookRetired` ARE the public contract of the Catalog
context. Borrowing subscribes to them (see
`infrastructure/event_handlers.py`) instead of importing `catalog.domain`
directly - this is what lets the two contexts evolve independently.
"""

from __future__ import annotations

from dataclasses import dataclass

from catalog.domain.value_objects import AuthorName, BookId, BookTitle, ISBN
from shared.domain_event import DomainEvent


@dataclass(frozen=True, kw_only=True)
class BookRegistered(DomainEvent):
    """A new title was added to the catalog and is ready to circulate."""

    book_id: BookId
    isbn: ISBN
    title: BookTitle
    author: AuthorName
    copies: int


@dataclass(frozen=True, kw_only=True)
class BookRetired(DomainEvent):
    """A title was withdrawn from the catalog (no longer acquired/re-shelved)."""

    book_id: BookId
    isbn: ISBN
