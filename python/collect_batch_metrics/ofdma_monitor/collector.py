"""Concurrent, ``asyncio``-based collection of one round's data for a single
metric: per-VMC CM MAC list -> per-CM metric fetch, all bounded by a single
``asyncio.Semaphore`` shared across every CLI call in the round.

This deliberately differs from the original bash structure ("one VMC at a
time up to MAX_PARALLEL_VMC, CMs within a VMC processed strictly serially"):
here every ``nomad alloc exec`` call -- whether it lists a VMC's CM MACs or
fetches one CM's metric data -- goes through the same semaphore, so
``max_parallel`` is simply "how many CLI calls may be in flight at once".
See docs/ARCHITECTURE.md for details.

The VMC list itself (``evc_client.get_vmc_status()``) is *not* fetched here:
it is fetched once per process lifetime by ``scheduler.py`` and reused by
every round, mirroring the original scripts calling ``gen_vmc_name_job`` once
before entering the ``while true`` loop.
"""

from __future__ import annotations

import asyncio
import logging

from .json_utils import JsonExtractionError
from .metrics.base import MetricPlugin
from .models import VmcInfo
from .nomad_client import NomadExecError
from .parsers.modem_brief import parse_modem_macs
from .storage import RoundStorage
from .vmc_client import VmcCliClient

logger = logging.getLogger(__name__)

#: Errors from a single CLI call that must never abort the whole round: a
#: `nomad alloc exec` failure/timeout (`NomadExecError`) or a response that
#: didn't contain a parseable JSON object (`JsonExtractionError` -- observed
#: in practice as an occasional empty stdout under concurrent load on the
#: same VMC session). One bad CM/VMC is logged and skipped for this round,
#: mirroring the original bash scripts (a failed CLI call there just leaves
#: an empty/missing file, never aborts the loop).
_CLI_CALL_ERRORS = (NomadExecError, JsonExtractionError)


async def _collect_one_cm(
    vmc_client: VmcCliClient,
    metric: MetricPlugin,
    storage: RoundStorage,
    round_no: int,
    vmc: VmcInfo,
    cm_mac: str,
    semaphore: asyncio.Semaphore,
) -> None:
    command = metric.build_command(cm_mac)
    async with semaphore:
        try:
            raw_json = await vmc_client.get_metric_data(vmc.job, command)
        except _CLI_CALL_ERRORS as exc:
            logger.warning(
                "metric=%s vmc=%s cm=%s: CLI call failed: %s",
                metric.name,
                vmc.name,
                cm_mac,
                exc,
            )
            return

    # Raw response is always kept, regardless of significance.
    storage.save_raw_response(round_no, vmc.name, cm_mac, raw_json)

    try:
        sample = metric.parse(vmc.name, cm_mac, raw_json)
    except Exception:  # noqa: BLE001 - one bad payload must not abort the round
        logger.exception(
            "metric=%s vmc=%s cm=%s: failed to parse response", metric.name, vmc.name, cm_mac
        )
        return

    if sample is not None and metric.is_significant(sample):
        storage.save_result(round_no, sample)


async def _collect_one_vmc(
    vmc_client: VmcCliClient,
    metric: MetricPlugin,
    storage: RoundStorage,
    round_no: int,
    vmc: VmcInfo,
    semaphore: asyncio.Semaphore,
) -> list[asyncio.Task[None]]:
    async with semaphore:
        try:
            raw_brief = await vmc_client.get_modem_brief(vmc.job)
        except _CLI_CALL_ERRORS as exc:
            logger.warning("vmc=%s: failed to list CM macs: %s", vmc.name, exc)
            return []

    cm_macs = parse_modem_macs(raw_brief)
    storage.save_cm_macs(round_no, vmc.name, cm_macs)
    logger.debug("vmc=%s: %d CM(s) to process", vmc.name, len(cm_macs))

    return [
        asyncio.create_task(
            _collect_one_cm(
                vmc_client, metric, storage, round_no, vmc, cm_mac, semaphore
            )
        )
        for cm_mac in cm_macs
    ]


async def collect_round(
    vmc_client: VmcCliClient,
    metric: MetricPlugin,
    storage: RoundStorage,
    round_no: int,
    vmcs: list[VmcInfo],
    max_parallel: int,
) -> None:
    """Collect one round of ``metric`` data across every VMC in ``vmcs``."""

    semaphore = asyncio.Semaphore(max_parallel)

    vmc_tasks = [
        asyncio.create_task(
            _collect_one_vmc(vmc_client, metric, storage, round_no, vmc, semaphore)
        )
        for vmc in vmcs
    ]
    per_vmc_cm_tasks = await asyncio.gather(*vmc_tasks)

    all_cm_tasks = [task for tasks in per_vmc_cm_tasks for task in tasks]
    if all_cm_tasks:
        await asyncio.gather(*all_cm_tasks)

    logger.info(
        "metric=%s round=%d: collected %d CM(s) across %d VMC(s)",
        metric.name,
        round_no,
        len(all_cm_tasks),
        len(vmcs),
    )
