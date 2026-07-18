"""Generic helpers for working with the JSON payloads returned by ncs_cli/confd_cli.

`ncs_cli` / `confd_cli` are interactive shells: the raw stdout captured by
`subprocess.run` is not guaranteed to be pure JSON. It typically contains the
echoed command(s), a shell prompt, and possibly the `unhide debug` banner
around the actual `| display json` payload. `extract_json_object` is the one
place that deals with that noise so every other module can assume it is
handed a clean ``dict``.
"""

from __future__ import annotations

import json
from collections.abc import Iterator
from typing import Any


class JsonExtractionError(ValueError):
    """Raised when no valid JSON object could be found in CLI output."""


def extract_json_object(raw_text: str) -> dict[str, Any]:
    """Find and decode the first complete top-level JSON object in ``raw_text``.

    Scans for each ``{`` occurrence and attempts ``json.JSONDecoder.raw_decode``
    from there. ``raw_decode`` stops as soon as a complete JSON value has been
    parsed, so trailing noise (e.g. a shell prompt printed after the payload)
    is naturally ignored. Leading noise (echoed commands, banners) is skipped
    by trying successive ``{`` positions until one parses successfully.
    """

    decoder = json.JSONDecoder()
    search_from = 0
    while True:
        start = raw_text.find("{", search_from)
        if start == -1:
            raise JsonExtractionError(
                "no JSON object found in CLI output: " + repr(raw_text[:200])
            )
        try:
            obj, _end = decoder.raw_decode(raw_text, start)
        except json.JSONDecodeError:
            search_from = start + 1
            continue
        if isinstance(obj, dict):
            return obj
        search_from = start + 1


def iter_mac_domain_modems(
    data: dict[str, Any], leaf_key: str
) -> Iterator[tuple[str, str, Any]]:
    """Walk the common ``data.ccap:ccap.docsis.docs-mac-domain.mac-domain[]``
    shape shared by ``cm_brief.json`` / ``cm_ofdma_mer.json`` / ``cm_uncorr.json``.

    Yields ``(mac_domain_name, modem_mac, leaf_value)`` for every modem entry
    that has ``leaf_key`` present (a mac-domain with no modems, or a modem
    missing the requested leaf, is silently skipped).
    """

    mac_domains = (
        data.get("data", {})
        .get("ccap:ccap", {})
        .get("docsis", {})
        .get("docs-mac-domain", {})
        .get("mac-domain", [])
    )
    for mac_domain in mac_domains:
        mac_domain_name = mac_domain.get("mac-domain-name", "")
        for modem in mac_domain.get("ccap-oper:modem", []):
            if leaf_key in modem:
                modem_mac = modem.get("modem-mac", "")
                yield mac_domain_name, modem_mac, modem[leaf_key]
