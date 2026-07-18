"""Parses `show ccap docsis docs-mac-domain mac-domain modem brief | display
json` (the ``cm_brief.json`` shape) into a flat, de-duplicated list of CM MAC
addresses. Replacement for ``gen_vmc_cm_macs``'s awk filtering.
"""

from __future__ import annotations

from typing import Any

from ..json_utils import iter_mac_domain_modems

BRIEF_LEAF_KEY = "brief"


def parse_modem_macs(data: dict[str, Any]) -> list[str]:
    macs: list[str] = []
    seen: set[str] = set()
    for _mac_domain_name, modem_mac, _brief in iter_mac_domain_modems(
        data, BRIEF_LEAF_KEY
    ):
        if modem_mac and modem_mac not in seen:
            seen.add(modem_mac)
            macs.append(modem_mac)
    return macs
