"""Xmit-chan-counter uncorrectable metric.

Sums the ``uncorrectable`` counter across a configurable set of OFDMA
channels (default 12/13). Replacement for
``gen_vmc_cm_xmit_chan_counter_data`` + ``count_cm_ofdma_uncorrectable_ch12_13``
in ``monitor_csl_ofdma_uncorrectable.sh``.
"""

from __future__ import annotations

from typing import Any

from ..json_utils import iter_mac_domain_modems
from ..models import MetricSample

LEAF_KEY = "xmit-chan-counter"


class XmitUncorrectableMetric:
    name = "xmit-chan-counter"
    rank_column = "total"

    def __init__(self, channels: tuple[int, ...] = (12, 13)) -> None:
        self.channels: tuple[int, ...] = tuple(channels)
        self.columns: tuple[str, ...] = tuple(
            f"ch{ch}_uncorr" for ch in self.channels
        ) + ("total",)

    def build_command(self, cm_mac: str) -> str:
        return (
            "show ccap docsis docs-mac-domain mac-domain modem "
            f"{cm_mac} xmit-chan-counter | display json"
        )

    def parse(
        self, vmc_name: str, cm_mac: str, raw_json: dict[str, Any]
    ) -> MetricSample | None:
        for _mac_domain_name, modem_mac, counters in iter_mac_domain_modems(
            raw_json, LEAF_KEY
        ):
            if modem_mac != cm_mac:
                continue
            per_channel = {ch: 0 for ch in self.channels}
            for entry in counters if isinstance(counters, list) else []:
                ucid = str(entry.get("ucid", ""))
                parts = ucid.split("/")
                if len(parts) != 2:
                    continue
                try:
                    channel = int(parts[1])
                except ValueError:
                    continue
                if channel not in per_channel:
                    continue
                try:
                    uncorrectable = int(entry.get("uncorrectable", 0))
                except (TypeError, ValueError):
                    uncorrectable = 0
                per_channel[channel] += uncorrectable

            total = sum(per_channel.values())
            values = {f"ch{ch}_uncorr": per_channel[ch] for ch in self.channels}
            values["total"] = total
            return MetricSample(
                vmc_name=vmc_name,
                cm_mac=cm_mac,
                values=values,
                rank_value=total,
            )
        return None

    def is_significant(self, sample: MetricSample) -> bool:
        return sample.values.get("total", 0) > 0
