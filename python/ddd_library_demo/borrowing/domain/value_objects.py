"""
Borrowing / Domain / Value Objects

Why an `ISBN` type is defined again here, instead of importing
`catalog.domain.value_objects.ISBN`
-------------------------------------------------------------------------
It would be technically easy to import Catalog's `ISBN` class. But doing
so would create a hidden coupling between the two bounded contexts' domain
layers - any change to Catalog's `ISBN` (e.g. tightening validation, or
one day dropping ISBN-10 support) would silently ripple into Borrowing.

Instead, each context models exactly the richness IT needs:
- Catalog owns bibliographic accuracy, so its `ISBN` enforces full
  ISBN-10/13 checksum validation.
- Borrowing only needs a stable, opaque key to look up "how many copies
  of this title are in circulation" - it never validates a checksum,
  because by the time Borrowing sees an ISBN (via the `BookRegistered`
  event), Catalog has already guaranteed it is well-formed.

This is a deliberate, small amount of duplication in exchange for context
independence - a very common, worthwhile trade-off in real DDD systems
(sometimes called "context-specific translation" of a shared concept, as
opposed to sharing a single class via a Shared Kernel).
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date, timedelta
from uuid import UUID

from shared.value_object import ValueObject

# Fixed lending-policy constant. In a larger system this might come from
# configuration; here it is a simple, explicit business rule.
STANDARD_LOAN_DURATION_DAYS = 14


@dataclass(frozen=True)
class ISBN(ValueObject):
    """Borrowing's own, deliberately minimal view of an ISBN (see module docstring)."""

    value: str

    def __post_init__(self) -> None:
        cleaned = self.value.replace("-", "").replace(" ", "").upper()
        if not cleaned:
            raise ValueError("ISBN must not be empty")
        object.__setattr__(self, "value", cleaned)

    def __str__(self) -> str:
        return self.value


@dataclass(frozen=True)
class LibraryId(ValueObject):
    """Identity of a `Library` aggregate (e.g. one per physical branch)."""

    value: UUID

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class MemberId(ValueObject):
    """Identity of a library member allowed to borrow/reserve books."""

    value: UUID

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class LoanId(ValueObject):
    """Identity of a single loan (one member borrowing one physical copy)."""

    value: UUID

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class ReservationId(ValueObject):
    """Identity of a single reservation (a member queued for a copy)."""

    value: UUID

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class LoanPeriod(ValueObject):
    """
    The time window during which a borrowed copy must be returned.

    Why this is a Value Object
    ------------------------------
    A `LoanPeriod` is fully described by its `borrowed_on`/`due_on` dates -
    two loans that happen to share the same dates are interchangeable for
    every purpose we care about (they are still attached to different
    `Loan` entities, which is where identity actually lives). Bundling the
    two dates together, plus the `is_overdue` query, avoids "primitive
    obsession" (passing two loose `date` values around everywhere) and
    keeps the loan-duration business rule in one place.
    """

    borrowed_on: date
    due_on: date

    def __post_init__(self) -> None:
        if self.due_on <= self.borrowed_on:
            raise ValueError("due_on must be after borrowed_on")

    @classmethod
    def starting(cls, borrowed_on: date) -> LoanPeriod:
        """Build a standard-length loan period starting on `borrowed_on`."""
        return cls(
            borrowed_on=borrowed_on,
            due_on=borrowed_on + timedelta(days=STANDARD_LOAN_DURATION_DAYS),
        )

    def is_overdue(self, as_of: date) -> bool:
        return as_of > self.due_on

    def days_overdue(self, as_of: date) -> int:
        return max(0, (as_of - self.due_on).days)
