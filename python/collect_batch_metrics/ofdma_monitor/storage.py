"""Persists raw CLI JSON responses and derived :class:`MetricSample` results
to disk, and provides read-back for :mod:`summary`.

Output layout (see docs/ARCHITECTURE.md for the full description)::

    output/<metric>/vmc_name_job.json
    output/<metric>/round<N>/<vmc_name>/cm_macs.json
    output/<metric>/round<N>/<vmc_name>/<vmc_name>_<mac_tag>.raw.json
    output/<metric>/round<N>/<vmc_name>/<vmc_name>_<mac_tag>.result.json
    output/<metric>/round<N>/summary_top<N>.json (+ .txt)   -- written by summary.py
    output/<metric>/common_cm.json (+ .txt)                 -- written by summary.py

``RoundStorage`` is the *only* module that knows about this on-disk layout.
Swapping to a different backend (SQLite, Parquet, ...) means rewriting only
this file; ``collector.py`` and ``summary.py`` only use the methods below.
"""

from __future__ import annotations

import json
import logging
from collections.abc import Iterator
from dataclasses import asdict
from pathlib import Path
from typing import Any

from .models import MetricSample, VmcInfo

logger = logging.getLogger(__name__)


def mac_tag(cm_mac: str) -> str:
    """`aa:bb:cc:dd:ee:ff` -> `aa_bb_cc_dd_ee_ff` (safe for filenames)."""

    return cm_mac.replace(":", "_")


class RoundStorage:
    """File-based storage rooted at ``<output_dir>/<metric_name>``."""

    def __init__(self, output_dir: Path | str, metric_name: str) -> None:
        self.metric_dir = Path(output_dir) / metric_name
        self.metric_dir.mkdir(parents=True, exist_ok=True)

    def round_dir(self, round_no: int) -> Path:
        return self.metric_dir / f"round{round_no}"

    def vmc_round_dir(self, round_no: int, vmc_name: str) -> Path:
        d = self.round_dir(round_no) / vmc_name
        d.mkdir(parents=True, exist_ok=True)
        return d

    # ---- vmc list: fetched once per process lifetime, shared by every round ----

    def save_vmc_list(self, vmcs: list[VmcInfo]) -> Path:
        path = self.metric_dir / "vmc_name_job.json"
        path.write_text(
            json.dumps([asdict(v) for v in vmcs], indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        logger.info("saved %d vmc entries to %s", len(vmcs), path)
        return path

    def load_vmc_list(self) -> list[VmcInfo]:
        path = self.metric_dir / "vmc_name_job.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        return [VmcInfo(**item) for item in data]

    def vmc_list_exists(self) -> bool:
        return (self.metric_dir / "vmc_name_job.json").exists()

    # ---- per-round, per-vmc CM mac list ----

    def save_cm_macs(self, round_no: int, vmc_name: str, cm_macs: list[str]) -> Path:
        path = self.vmc_round_dir(round_no, vmc_name) / "cm_macs.json"
        path.write_text(json.dumps(cm_macs, indent=2), encoding="utf-8")
        return path

    # ---- raw CLI response + parsed result, one pair per CM per round ----

    def save_raw_response(
        self, round_no: int, vmc_name: str, cm_mac: str, raw_json: dict[str, Any]
    ) -> Path:
        """Always called, regardless of whether the metric turned out
        significant -- this is the "keep every round's raw CLI JSON" behavior
        requested for the Python reimplementation.
        """

        path = (
            self.vmc_round_dir(round_no, vmc_name)
            / f"{vmc_name}_{mac_tag(cm_mac)}.raw.json"
        )
        path.write_text(
            json.dumps(raw_json, indent=2, ensure_ascii=False), encoding="utf-8"
        )
        return path

    def save_result(self, round_no: int, sample: MetricSample) -> Path:
        """Only called when `MetricPlugin.is_significant(sample)` is True,
        mirroring the original scripts only writing a `*_count.txt` file
        when the count was non-zero.
        """

        path = (
            self.vmc_round_dir(round_no, sample.vmc_name)
            / f"{sample.vmc_name}_{mac_tag(sample.cm_mac)}.result.json"
        )
        path.write_text(json.dumps(asdict(sample), indent=2), encoding="utf-8")
        return path

    def iter_round_results(self, round_no: int) -> Iterator[MetricSample]:
        """Scan `*.result.json` files for one round, replacing the original
        `find "$save_dir" -mindepth 2 -maxdepth 2 -type f -name '*_count.txt'`.
        """

        round_dir = self.round_dir(round_no)
        if not round_dir.exists():
            return
        for result_file in sorted(round_dir.glob("*/*.result.json")):
            data = json.loads(result_file.read_text(encoding="utf-8"))
            yield MetricSample(**data)
