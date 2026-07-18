"""Ranking and cross-round aggregation.

Replaces two pieces of awk in the original scripts:

- ``summarize_round_top_counts``: rank one round's significant CMs and keep
  the Top-N.
- ``update_common_cm_summary``: find every ``(vmc, cm_mac)`` pair that shows
  up in the Top-N summary of *every* round processed so far (the original's
  ``FNR==1``/``SUBSEP`` multi-file join trick, replaced with a plain dict).
"""

from __future__ import annotations

import json
import logging

from .models import CommonCmEntry, MetricSample
from .storage import RoundStorage

logger = logging.getLogger(__name__)

_VMC_WIDTH = 40
_MAC_WIDTH = 20
_COLUMN_WIDTH = 12


def _format_row(cells: list[str], widths: list[int]) -> str:
    """Mimics `printf '%-Ws %-Ws ... %s\\n'`: every cell but the last is
    left-padded to its width; the last cell is emitted as-is.
    """

    parts = [cell.ljust(w) for cell, w in zip(cells[:-1], widths[:-1])]
    parts.append(cells[-1])
    return " ".join(parts)


def _sample_to_dict(sample: MetricSample) -> dict:
    return {
        "vmc_name": sample.vmc_name,
        "cm_mac": sample.cm_mac,
        "values": sample.values,
        "rank_value": sample.rank_value,
    }


def _render_round_table(rows: list[MetricSample], columns: tuple[str, ...]) -> str:
    widths = [_VMC_WIDTH, _MAC_WIDTH] + [_COLUMN_WIDTH] * len(columns)
    header = _format_row(["VMC", "CM_MAC", *[c.upper() for c in columns]], widths)
    lines = [header]
    for row in rows:
        cells = [row.vmc_name, row.cm_mac, *[str(row.values.get(c, 0)) for c in columns]]
        lines.append(_format_row(cells, widths))
    return "\n".join(lines) + "\n"


def summarize_round(
    storage: RoundStorage,
    round_no: int,
    top_n: int,
    columns: tuple[str, ...],
) -> list[MetricSample]:
    """Rank one round's significant CMs and persist the Top-N.

    Replacement for `summarize_round_top_counts`.
    """

    samples = list(storage.iter_round_results(round_no))
    samples.sort(key=lambda s: s.rank_value, reverse=True)
    top_rows = samples[:top_n]

    round_dir = storage.round_dir(round_no)
    round_dir.mkdir(parents=True, exist_ok=True)
    json_path = round_dir / f"summary_top{top_n}.json"
    txt_path = round_dir / f"summary_top{top_n}.txt"
    json_path.write_text(
        json.dumps([_sample_to_dict(r) for r in top_rows], indent=2), encoding="utf-8"
    )
    txt_path.write_text(_render_round_table(top_rows, columns), encoding="utf-8")
    logger.info(
        "round %d: %d significant CMs found, wrote top-%d summary to %s",
        round_no,
        len(samples),
        top_n,
        json_path,
    )
    return top_rows


def _common_entry_to_dict(entry: CommonCmEntry) -> dict:
    return {
        "vmc_name": entry.vmc_name,
        "cm_mac": entry.cm_mac,
        "per_round_values": {str(k): v for k, v in entry.per_round_values.items()},
        "last_rank_value": entry.last_rank_value,
    }


def _render_common_table(
    entries: list[CommonCmEntry], columns: tuple[str, ...]
) -> str:
    widths = [_VMC_WIDTH, _MAC_WIDTH]
    header = _format_row(["VMC", "CM_MAC", "PER_ROUND_VALUES"], widths + [0])
    lines = [header]
    for entry in entries:
        per_round = ",".join(
            f"round{round_no}=" + "/".join(str(values.get(c, 0)) for c in columns)
            for round_no, values in sorted(entry.per_round_values.items())
        )
        lines.append(_format_row([entry.vmc_name, entry.cm_mac, per_round], widths + [0]))
    return "\n".join(lines) + "\n"


def update_common_cm_summary(
    storage: RoundStorage,
    current_round: int,
    top_n: int,
    columns: tuple[str, ...],
) -> list[CommonCmEntry] | None:
    """Find every ``(vmc, cm_mac)`` pair present in the Top-N summary of
    every round from 1..``current_round`` (only rounds whose summary file
    exists are counted, exactly like the original's ``[[ -e "$f" ]]`` guard).

    Replacement for `update_common_cm_summary`.
    """

    round_summaries: dict[int, list[MetricSample]] = {}
    for round_no in range(1, current_round + 1):
        json_path = storage.round_dir(round_no) / f"summary_top{top_n}.json"
        if not json_path.exists():
            continue
        data = json.loads(json_path.read_text(encoding="utf-8"))
        round_summaries[round_no] = [MetricSample(**item) for item in data]

    if not round_summaries:
        return None

    total_rounds = len(round_summaries)
    per_cm_rounds: dict[tuple[str, str], dict[int, dict[str, int]]] = {}
    last_rank_value: dict[tuple[str, str], int] = {}
    for round_no, samples in round_summaries.items():
        for sample in samples:
            key = (sample.vmc_name, sample.cm_mac)
            per_cm_rounds.setdefault(key, {})[round_no] = sample.values
            last_rank_value[key] = sample.rank_value

    entries = [
        CommonCmEntry(
            vmc_name=vmc_name,
            cm_mac=cm_mac,
            per_round_values=rounds_seen,
            last_rank_value=last_rank_value[(vmc_name, cm_mac)],
        )
        for (vmc_name, cm_mac), rounds_seen in per_cm_rounds.items()
        if len(rounds_seen) == total_rounds
    ]
    entries.sort(key=lambda e: e.last_rank_value, reverse=True)

    json_path = storage.metric_dir / "common_cm.json"
    txt_path = storage.metric_dir / "common_cm.txt"
    json_path.write_text(
        json.dumps([_common_entry_to_dict(e) for e in entries], indent=2),
        encoding="utf-8",
    )
    txt_path.write_text(_render_common_table(entries, columns), encoding="utf-8")
    logger.info(
        "updated common_cm summary: %d CM(s) common to all %d round(s) -> %s",
        len(entries),
        total_rounds,
        json_path,
    )
    return entries
