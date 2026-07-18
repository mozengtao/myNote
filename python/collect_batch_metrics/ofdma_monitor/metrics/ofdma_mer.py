"""OFDMA sub-carrier MER metric.

Counts sub-carriers whose RxMER is below a configurable threshold (default
40 dB) across all channels reported for a CM. Replacement for
``gen_vmc_cm_ofdma_mer_data`` + ``count_cm_ofmda_mer_below40`` in
``monitor_csl_ofdma_mer.sh``.
"""

from __future__ import annotations

from typing import Any

from ..json_utils import iter_mac_domain_modems
from ..models import MetricSample
from ..parsers.merdata_table import count_below_threshold

LEAF_KEY = "ofdma-sub-carrier-mer"


class OfdmaMerMetric:
    name = "ofdma-sub-carrier-mer"
    columns: tuple[str, ...] = ("count",)
    rank_column = "count"

    def __init__(self, threshold_db: float = 40.0) -> None:
        self.threshold_db = threshold_db

    def build_command(self, cm_mac: str) -> str:
        return (
            "show ccap docsis docs-mac-domain mac-domain modem "
            f"{cm_mac} ofdma-sub-carrier-mer merdata | display json"
        )

    def parse(
        self, vmc_name: str, cm_mac: str, raw_json: dict[str, Any]
    ) -> MetricSample | None:
        for _mac_domain_name, modem_mac, leaf in iter_mac_domain_modems(
            raw_json, LEAF_KEY
        ):
            if modem_mac != cm_mac:
                continue
            merdata = leaf.get("merdata", "") if isinstance(leaf, dict) else ""
            count = count_below_threshold(merdata, self.threshold_db)
            return MetricSample(
                vmc_name=vmc_name,
                cm_mac=cm_mac,
                values={"count": count},
                rank_value=count,
            )
        return None

    def is_significant(self, sample: MetricSample) -> bool:
        return sample.values.get("count", 0) != 0
