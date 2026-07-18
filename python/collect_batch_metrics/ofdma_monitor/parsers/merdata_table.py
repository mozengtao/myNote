"""Parses the ASCII RxMER table embedded in the ``merdata`` string leaf
(see ``cm_ofdma_mer.json``). The leaf itself is plain text, not structured
JSON, e.g.::

    Timestamp: 2026-07-18T02:59:11+00:00
    RxMER data (dB) for channel 12 sub-carrier[148 --> 1907], avg 37.50 dB:
    148(40.50MHz): 42.75 38.75 ... old= 44.75 41.25 ...
    158(40.75MHz): 38.75 38.25 ... old= 42.25 42.00 ...

Replicates ``count_cm_ofmda_mer_below40``'s awk logic: only count values in
the "current" half of each row (before the ``old=`` marker), on rows whose
first token looks like ``<subcarrier>(<freq>MHz):``.
"""

from __future__ import annotations

import re

_HEADER_MARKER = "RxMER data (dB) for channel"
_ROW_FIRST_FIELD = re.compile(r"^\d+\(.*MHz\):?$")


def count_below_threshold(merdata_text: str, threshold_db: float = 40.0) -> int:
    """Count sub-carrier RxMER samples below ``threshold_db`` in ``merdata_text``.

    Returns 0 if the text doesn't contain the expected header (mirrors the
    original script's ``grep -q 'RxMER data (dB) for channel'`` guard).
    """

    if _HEADER_MARKER not in merdata_text:
        return 0

    count = 0
    for line in merdata_text.splitlines():
        fields = line.split()
        if not fields or not _ROW_FIRST_FIELD.match(fields[0]):
            continue
        for token in fields[1:]:
            if "old" in token:
                break
            try:
                value = float(token)
            except ValueError:
                continue
            if value < threshold_db:
                count += 1
    return count
