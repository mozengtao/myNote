"""
Infrastructure / IdGenerator implementation
"""

from __future__ import annotations

from uuid import UUID, uuid4

from shared.id_generator import IdGenerator


class UuidIdGenerator(IdGenerator):
    """
    Concrete `IdGenerator` producing random UUID4 values.

    Why this is Infrastructure, not Domain
    -------------------------------------------
    "How" an id is generated (random UUID4, sequential, a remote id
    service...) is a technical decision with no business meaning -
    exactly the kind of decision the Dependency Inversion Principle says
    should live at the outermost layer, behind the `IdGenerator` interface
    declared in `shared/id_generator.py`.
    """

    def new_id(self) -> UUID:
        return uuid4()
