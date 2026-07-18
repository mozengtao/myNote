"""Parses `show vmc status | display json` (the ``vmc_status.json`` shape)
into :class:`~ofdma_monitor.models.VmcInfo` objects.

Replicates the filtering performed by the original ``gen_vmc_name_job`` awk
one-liner::

    $2 ~ /vmc-/ && $1 !~ /standby/ && $1 !~ /graphite/ && $NF !~ /hot/

**Verified against a live system (see docs/ARCHITECTURE.md "known
assumptions")**: ``vmc.name`` (e.g. ``"vmc-morris-dentist-1"``) is *not* the
Nomad job name -- passing it as ``-job`` to `nomad alloc exec` fails with
``No job(s) with prefix or ID "..." found``. The real job name is the
*full* ``status.container-name`` value, including its trailing allocation
UUID (e.g. ``"vmc-evc-dentist-31432bd8-8ef9-45e3-97fb-dbdf108d4f11"``) --
confirmed by running
``nomad alloc exec -task vmc -job vmc-evc-dentist-31432bd8-8ef9-45e3-97fb-dbdf108d4f11 sh``
directly against a live system.

``status.ha-status`` is treated as the "hot standby" indicator that used to
be the tab table's last column -- this part is still unverified (no real
standby/graphite/hot record has been seen yet).
"""

from __future__ import annotations

import logging
from typing import Any

from ..models import VmcInfo

logger = logging.getLogger(__name__)

_EXCLUDE_NAME_SUBSTRINGS = ("standby", "graphite")
_EXCLUDE_HA_STATUS_SUBSTRINGS = ("hot",)


def _job_from_container_name(container_name: str, fallback: str) -> str:
    """The Nomad job name is the full `status.container-name` value (UUID
    suffix included). Falls back to `fallback` (and logs a warning) only if
    `container_name` is missing entirely.
    """

    if not container_name:
        logger.warning(
            "vmc has no status.container-name; falling back to %r as the "
            "Nomad job name (likely wrong, see docs/ARCHITECTURE.md)",
            fallback,
        )
        return fallback
    return container_name


def is_included(vmc: dict[str, Any]) -> bool:
    """Mirrors `$1 !~ /standby/ && $1 !~ /graphite/ && $NF !~ /hot/`."""

    name = vmc.get("name", "")
    ha_status = str(vmc.get("status", {}).get("ha-status", ""))

    if any(substr in name for substr in _EXCLUDE_NAME_SUBSTRINGS):
        return False
    if any(substr in ha_status for substr in _EXCLUDE_HA_STATUS_SUBSTRINGS):
        return False
    return True


def parse_vmc_status(data: dict[str, Any]) -> list[VmcInfo]:
    """Parse the full `show vmc status | display json` payload."""

    vmcs = (
        data.get("data", {})
        .get("vcm-vmc-mapper-svc:vmc-state", {})
        .get("vmc", [])
    )

    result: list[VmcInfo] = []
    for vmc in vmcs:
        if not is_included(vmc):
            continue
        name = vmc.get("name", "")
        status = vmc.get("status", {})
        job = _job_from_container_name(str(status.get("container-name", "")), fallback=name)
        result.append(
            VmcInfo(
                name=name,
                job=job,
                state=str(status.get("state", "")),
                internal_state=str(status.get("internal-state", "")),
                ha_status=str(status.get("ha-status", "")),
            )
        )
    return result
