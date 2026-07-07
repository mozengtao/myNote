"""
Value Object base

DDD Concept: Value Object
-------------------------
A Value Object (VO) models a concept that matters only for WHAT it is, never
WHICH one it is. Two VOs with the same attributes are completely
interchangeable - there is no hidden identity ticket attached to them.

Contrast with Entity: an `ISBN("978-0134685991")` is always equal to another
`ISBN("978-0134685991")`. There is no "this particular ISBN instance" - only
the value. A `Book`, on the other hand, has identity: two books can share the
same title yet remain distinct because they have different `BookId`s.

Value Objects in this project:
- are implemented as `@dataclass(frozen=True)` (immutable - `frozen=True`
  raises on attempted mutation, and dataclasses generate `__eq__` for us,
  giving "compare by value" for free)
- validate their own invariants in `__post_init__` so an invalid VO can
  never exist ("make illegal states unrepresentable")

This module only documents the convention; Python's `dataclasses` module
already gives us everything we need; there is no base class to inherit
from other than the marker below, which exists purely for readability
and `isinstance` checks in tests.
"""

from __future__ import annotations


class ValueObject:
    """
    Marker base class for Value Objects.

    Why it exists
    -------------
    Purely documentational/typing convenience so that code reading
    `isinstance(x, ValueObject)` or a type hint of `ValueObject` makes the
    DDD intent explicit. All real behaviour (immutability, value equality,
    validation) comes from concrete subclasses being declared as
    `@dataclass(frozen=True)`.

    Why it belongs here (shared kernel)
    ------------------------------------
    Both the Catalog and Borrowing contexts define their own Value Objects
    (ISBN, MemberId, ...), but the *concept* of "a Value Object" is
    universal DDD vocabulary, not something either context owns.
    """

    __slots__ = ()
