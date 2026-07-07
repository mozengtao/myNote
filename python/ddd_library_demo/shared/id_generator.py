"""
Shared / IdGenerator interface

Why this exists
----------------
Application Services need to mint new identities (`BookId`, `LibraryId`,
`LoanId`, ...) when creating aggregates. Deciding HOW an id is produced
(random UUID4, a database sequence, a distributed id service, ...) is an
Infrastructure concern, and it also affects testability - tests want
deterministic, predictable ids. Defining this tiny `Protocol` lets
Application Services depend only on "give me a fresh UUID", while
`infrastructure/id_generator.py` supplies the real (random) implementation
and tests can supply a fake, fully-predictable one if needed.

Why it lives in the Shared Kernel rather than each context's `domain/`
--------------------------------------------------------------------------
Both bounded contexts need identical id-generation behaviour (a random
UUID) and there is no context-specific rule involved - unlike
`BookRepository` or `LibraryRepository`, which are shaped by each
context's own aggregate.
"""

from __future__ import annotations

from typing import Protocol
from uuid import UUID


class IdGenerator(Protocol):
    def new_id(self) -> UUID:
        """Return a fresh, globally-unique identifier."""
        ...
