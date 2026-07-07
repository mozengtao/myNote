"""
Borrowing / Domain / Repository interface

See `catalog/domain/repository.py` for the full explanation of why this
interface lives in the Domain layer and why it is a `Protocol`. The same
reasoning applies here.
"""

from __future__ import annotations

from typing import Protocol

from borrowing.domain.entities import Library
from borrowing.domain.value_objects import LibraryId


class LibraryNotFoundError(Exception):
    """Raised when a repository lookup finds no matching Library."""


class LibraryRepository(Protocol):
    def save(self, library: Library) -> None:
        """Persist the current state of `library` (insert or update)."""
        ...

    def get_by_id(self, library_id: LibraryId) -> Library:
        """Return the Library with `library_id`, or raise LibraryNotFoundError."""
        ...

    def list_all(self) -> list[Library]:
        """Return every Library instance known to this repository."""
        ...
