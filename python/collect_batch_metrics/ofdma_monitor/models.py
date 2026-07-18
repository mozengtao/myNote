"""Plain dataclass models shared across the collector / storage / summary layers.

All models are intentionally simple, immutable value objects. They carry no
behaviour of their own; parsing logic lives in ``parsers/`` and ``metrics/``,
persistence lives in ``storage.py``.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class VmcInfo:
    """One entry from `show vmc status` (vmc_status.json)."""

    name: str
    job: str
    state: str = ""
    internal_state: str = ""
    ha_status: str = ""


@dataclass(frozen=True)
class MetricSample:
    """Result of applying a single MetricPlugin to one CM in one round.

    ``values`` holds the plugin-specific named counters, e.g. ``{"count": 5}``
    for the MER metric or ``{"ch12_uncorr": 3, "ch13_uncorr": 0, "total": 3}``
    for the uncorrectable metric. ``rank_value`` is the single integer used
    for sorting / Top-N (taken from ``MetricPlugin.rank_column``).
    """

    vmc_name: str
    cm_mac: str
    values: dict[str, int] = field(default_factory=dict)
    rank_value: int = 0


@dataclass(frozen=True)
class RoundSummaryRow:
    """One ranked row in a round's `summary_top<N>` output."""

    vmc_name: str
    cm_mac: str
    values: dict[str, int]
    rank_value: int


@dataclass(frozen=True)
class CommonCmEntry:
    """A (vmc, cm_mac) pair that appeared in every round processed so far."""

    vmc_name: str
    cm_mac: str
    per_round_values: dict[int, dict[str, int]]
    last_rank_value: int
