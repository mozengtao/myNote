"""
Borrowing / Domain / Services

DDD Concept: Domain Service
--------------------------------
A Domain Service holds a piece of business logic that is real domain
knowledge (it belongs in the Domain layer, not the Application layer) but
does NOT naturally belong to any single Entity or Value Object.

Why `LateFeePolicy` is a Domain Service and not a method on `Loan`
-----------------------------------------------------------------------
It is tempting to write `loan.calculate_fee()`. But a late fee depends on
information that has nothing to do with "what is a Loan" - it depends on a
FEE SCHEDULE (money per day overdue), which is a pricing/business policy
that:
- can change independently of the lending process itself (the library
  board might change the daily fee next month without anything about how
  loans work changing),
- may legitimately differ per library branch, membership tier, or
  promotion, i.e. it is a *policy* that gets applied TO a loan, not an
  intrinsic property OF a loan,
- would force `Loan` to either hardcode a fee schedule (making it
  inflexible) or accept a schedule parameter on every call (which is
  really just... a service, wearing an Entity's clothes).

Because the computation needs a `Loan` (Entity) plus an external policy
(the fee schedule) plus "today's date", and produces a derived value
rather than changing anyone's state, it is a textbook Domain Service: a
stateless operation over one or more domain objects that does not fit
inside any of them.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from decimal import Decimal

from borrowing.domain.entities import Loan


@dataclass(frozen=True)
class FeeSchedule:
    """The policy input to `LateFeePolicy`: how much is charged per overdue day."""

    daily_rate: Decimal
    max_fee: Decimal


class LateFeePolicy:
    """Computes the late fee owed for an overdue loan under a given fee schedule."""

    def __init__(self, schedule: FeeSchedule) -> None:
        self._schedule = schedule

    def calculate_fee(self, loan: Loan, as_of: date) -> Decimal:
        days_overdue = loan.period.days_overdue(as_of)
        if days_overdue == 0:
            return Decimal("0.00")
        fee = self._schedule.daily_rate * days_overdue
        return min(fee, self._schedule.max_fee)
