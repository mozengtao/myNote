"""
Tests demonstrating Value Object behaviour: immutability, value equality,
and self-validation.
"""

from __future__ import annotations

from datetime import date

import pytest

from borrowing.domain.value_objects import LoanPeriod
from catalog.domain.value_objects import ISBN, AuthorName, BookTitle


class TestISBN:
    def test_equal_by_value_not_identity(self) -> None:
        assert ISBN("978-0-13-468599-1") == ISBN("9780134685991")

    def test_normalizes_dashes_and_spaces(self) -> None:
        assert ISBN("978-0134685991").value == "9780134685991"

    def test_rejects_invalid_checksum(self) -> None:
        with pytest.raises(ValueError):
            ISBN("9780134685999")

    def test_is_immutable(self) -> None:
        isbn = ISBN("9780134685991")
        with pytest.raises(Exception):
            isbn.value = "9780321125217"  # type: ignore[misc]


class TestBookTitle:
    def test_rejects_blank_title(self) -> None:
        with pytest.raises(ValueError):
            BookTitle("   ")

    def test_strips_surrounding_whitespace(self) -> None:
        assert BookTitle("  Clean Code  ").value == "Clean Code"


class TestAuthorName:
    def test_rejects_blank_author(self) -> None:
        with pytest.raises(ValueError):
            AuthorName("")


class TestLoanPeriod:
    def test_standard_duration_is_fourteen_days(self) -> None:
        period = LoanPeriod.starting(date(2026, 1, 1))
        assert period.due_on == date(2026, 1, 15)

    def test_due_on_must_be_after_borrowed_on(self) -> None:
        with pytest.raises(ValueError):
            LoanPeriod(borrowed_on=date(2026, 1, 15), due_on=date(2026, 1, 1))

    def test_is_overdue_true_after_due_date(self) -> None:
        period = LoanPeriod.starting(date(2026, 1, 1))
        assert period.is_overdue(date(2026, 1, 16)) is True
        assert period.is_overdue(date(2026, 1, 15)) is False

    def test_days_overdue_counts_from_due_date(self) -> None:
        period = LoanPeriod.starting(date(2026, 1, 1))
        assert period.days_overdue(date(2026, 1, 20)) == 5
        assert period.days_overdue(date(2026, 1, 10)) == 0
