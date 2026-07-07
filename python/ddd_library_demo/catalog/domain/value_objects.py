"""
Catalog / Domain / Value Objects

Why these are Value Objects, not Entities
-------------------------------------------
`ISBN`, `BookTitle`, and `AuthorName` are all defined completely by their
value. Two `ISBN("978-0134685991")` instances are, for every purpose in
this system, THE SAME ISBN - there is no scenario where we need to tell
"this occurrence of the ISBN" apart from "that occurrence". That is
precisely the test for "should this be a Value Object?": if identity never
matters, only content does, it is a Value Object.

They are also where format/business validation for these primitives lives,
so an invalid `ISBN` can never even be constructed ("make illegal states
unrepresentable") - callers do not need to repeatedly re-validate raw
strings throughout the codebase.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from uuid import UUID

from shared.value_object import ValueObject

# ISBN-13 (post-2007) uses this prefix; ISBN-10 is still accepted for
# older catalog entries, which is realistic for a library system.
_ISBN13_RE = re.compile(r"^(978|979)\d{9}[0-9]$")
_ISBN10_RE = re.compile(r"^\d{9}[\dX]$")


def _digits_only(raw: str) -> str:
    return raw.replace("-", "").replace(" ", "").upper()


def _is_valid_isbn10(digits: str) -> bool:
    if not _ISBN10_RE.match(digits):
        return False
    total = 0
    for position, char in enumerate(digits):
        value = 10 if char == "X" else int(char)
        total += (10 - position) * value
    return total % 11 == 0


def _is_valid_isbn13(digits: str) -> bool:
    if not _ISBN13_RE.match(digits):
        return False
    total = sum(
        int(char) * (1 if position % 2 == 0 else 3)
        for position, char in enumerate(digits)
    )
    return total % 10 == 0


@dataclass(frozen=True)
class BookId(ValueObject):
    """
    The technical (surrogate) identity of a `Book` Entity.

    Why a separate identity from `ISBN`
    --------------------------------------
    It is tempting to just use the ISBN as the Entity's identity - and in
    many real systems that would be a fine natural key. This project
    deliberately keeps them distinct to make an important DDD point
    explicit: Entity identity is a technical concept ("which row/object is
    this?") and can be completely independent of any business attribute
    ("what is this thing called?"). Using a UUID-backed `BookId` means
    that even if cataloging rules around ISBNs changed (e.g. supporting
    books that predate ISBNs), the Entity's identity would be unaffected.
    """

    value: UUID

    def __str__(self) -> str:
        return str(self.value)


@dataclass(frozen=True)
class ISBN(ValueObject):
    """
    The International Standard Book Number identifying a title.

    Why a Value Object
    -------------------
    An ISBN has no lifecycle of its own - it never changes, is never
    "updated", and two equal ISBNs are indistinguishable. It is also the
    natural key the Borrowing context uses to refer to a catalog title
    without needing a direct object reference (see `catalog/__init__.py`
    on Bounded Context boundaries).
    """

    value: str

    def __post_init__(self) -> None:
        digits = _digits_only(self.value)
        if not (_is_valid_isbn10(digits) or _is_valid_isbn13(digits)):
            raise ValueError(f"'{self.value}' is not a valid ISBN-10 or ISBN-13")
        # `frozen=True` disallows `self.value = ...` directly, so normalize
        # via `object.__setattr__` - the one sanctioned escape hatch for
        # doing validation/normalization work inside an immutable dataclass.
        object.__setattr__(self, "value", digits)

    def __str__(self) -> str:
        return self.value


@dataclass(frozen=True)
class BookTitle(ValueObject):
    """A title's text, validated to never be blank or absurdly long."""

    value: str

    def __post_init__(self) -> None:
        cleaned = self.value.strip()
        if not cleaned:
            raise ValueError("Book title must not be empty")
        if len(cleaned) > 200:
            raise ValueError("Book title must not exceed 200 characters")
        object.__setattr__(self, "value", cleaned)

    def __str__(self) -> str:
        return self.value


@dataclass(frozen=True)
class AuthorName(ValueObject):
    """An author's display name, validated to never be blank."""

    value: str

    def __post_init__(self) -> None:
        cleaned = self.value.strip()
        if not cleaned:
            raise ValueError("Author name must not be empty")
        object.__setattr__(self, "value", cleaned)

    def __str__(self) -> str:
        return self.value
