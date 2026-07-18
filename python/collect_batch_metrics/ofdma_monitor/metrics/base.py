"""Defines the ``MetricPlugin`` protocol shared by every pluggable per-CM
metric (``ofdma-sub-carrier-mer``, ``xmit-chan-counter`` uncorrectable, and
any metric added in the future).

``collector.py`` / ``storage.py`` / ``summary.py`` / ``scheduler.py`` only
depend on this protocol, never on a concrete metric class. Adding a new
monitor is therefore a matter of writing one new ``metrics/xxx.py`` module
implementing this protocol and registering it in ``cli.py`` -- no other
module needs to change. See docs/ARCHITECTURE.md for the step-by-step guide.
"""

from __future__ import annotations

from typing import Any, Protocol, runtime_checkable

from ..models import MetricSample


@runtime_checkable
class MetricPlugin(Protocol):
    #: Output sub-directory name under the configured output root, e.g.
    #: ``"ofdma-sub-carrier-mer"``.
    name: str

    #: Ordered column names used when rendering the human-readable summary
    #: table, e.g. ``("count",)`` or ``("ch12_uncorr", "ch13_uncorr", "total")``.
    columns: tuple[str, ...]

    #: Which key in ``MetricSample.values`` is used for ranking / Top-N.
    rank_column: str

    def build_command(self, cm_mac: str) -> str:
        """Build the confd_cli command to fetch this metric for one CM."""
        ...

    def parse(
        self, vmc_name: str, cm_mac: str, raw_json: dict[str, Any]
    ) -> MetricSample | None:
        """Parse the JSON payload returned by `build_command` into a sample.

        Returns ``None`` if the expected leaf is missing from the payload
        (e.g. the CM has no data for this metric).
        """
        ...

    def is_significant(self, sample: MetricSample) -> bool:
        """Whether this sample is "interesting" enough to keep a `.result.json`
        for (mirrors the original scripts only writing a `*_count.txt` file
        when the count was non-zero). Raw JSON is always kept regardless.
        """
        ...
