"""Periodic round scheduler with drift-corrected sleeping and graceful
signal handling.

Replaces both the ``while true; ...; sleep 600; done`` loop inside each bash
monitor script *and* ``run_ofdma_monitors.sh``'s "two background processes +
``trap`` + ``wait``" pattern: every configured metric runs as one ``asyncio``
task inside a single process/event loop, and SIGINT/SIGTERM are handled once,
centrally, for all of them.

The VMC list is fetched exactly once per metric, before the loop starts
(mirroring the original ``gen_vmc_name_job > vmc_name_job.txt`` call that
happens once in ``main()``, *before* the ``while true`` loop -- every round
re-reads that same file rather than re-querying `show vmc status`).
"""

from __future__ import annotations

import asyncio
import logging
import signal
import time
from pathlib import Path

from .collector import collect_round
from .evc_client import EvcCliClient
from .metrics.base import MetricPlugin
from .models import VmcInfo
from .parsers.vmc_status import parse_vmc_status
from .storage import RoundStorage
from .summary import summarize_round, update_common_cm_summary
from .vmc_client import VmcCliClient

logger = logging.getLogger(__name__)


async def fetch_vmc_list(evc_client: EvcCliClient) -> list[VmcInfo]:
    """Replacement for `gen_vmc_name_job`'s full pipeline (CLI call + filter)."""

    raw = await evc_client.get_vmc_status()
    return parse_vmc_status(raw)


async def run_metric_loop(
    metric: MetricPlugin,
    evc_client: EvcCliClient,
    vmc_client: VmcCliClient,
    output_dir: Path,
    interval: int,
    max_parallel: int,
    top_n: int,
    stop_event: asyncio.Event,
    once: bool = False,
) -> None:
    """Equivalent of one bash monitor script's ``main()``."""

    storage = RoundStorage(output_dir, metric.name)

    try:
        vmcs = await fetch_vmc_list(evc_client)
    except Exception:
        # Without a VMC list this metric's loop cannot do anything, but a
        # single failed startup call must not cascade into killing the
        # *other* metric's loop via asyncio.gather in run_all.
        logger.exception(
            "metric=%s: failed to fetch the initial VMC list; this metric "
            "will not run (other metrics are unaffected)",
            metric.name,
        )
        return
    storage.save_vmc_list(vmcs)
    logger.info("metric=%s: monitoring %d VMC(s)", metric.name, len(vmcs))

    round_no = 0
    next_deadline = time.monotonic()
    while not stop_event.is_set():
        next_deadline += interval
        round_no += 1
        logger.info("--- metric=%s round=%d: collecting ---", metric.name, round_no)

        try:
            await collect_round(vmc_client, metric, storage, round_no, vmcs, max_parallel)
            summarize_round(storage, round_no, top_n, metric.columns)
            update_common_cm_summary(storage, round_no, top_n, metric.columns)
        except Exception:
            # Individual CLI-call failures are already handled inside
            # collect_round; this is a last-resort safety net for anything
            # unexpected so one bad round doesn't kill this metric's loop
            # (and, via asyncio.gather in run_all, every other metric too).
            logger.exception(
                "metric=%s round=%d: unexpected error during collection; "
                "will retry next round",
                metric.name,
                round_no,
            )

        if once:
            logger.info("metric=%s: --once requested, exiting after round %d", metric.name, round_no)
            return

        sleep_for = max(0.0, next_deadline - time.monotonic())
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=sleep_for)
        except asyncio.TimeoutError:
            pass  # normal case: the interval elapsed, loop again


async def run_all(
    metrics: list[MetricPlugin],
    evc_client: EvcCliClient,
    vmc_client: VmcCliClient,
    output_dir: Path,
    interval: int,
    max_parallel: int,
    top_n: int,
    once: bool = False,
) -> None:
    """Run every configured metric's periodic loop concurrently in one
    process, with SIGINT/SIGTERM triggering one shared graceful shutdown.
    """

    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()

    def _request_stop(sig_name: str) -> None:
        logger.info(
            "received %s, finishing in-flight round(s) then shutting down...",
            sig_name,
        )
        stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, _request_stop, sig.name)
        except NotImplementedError:
            # add_signal_handler is only supported on Unix event loops;
            # fall back to default signal handling elsewhere.
            logger.debug("signal handler for %s not supported on this platform", sig)

    tasks = [
        asyncio.create_task(
            run_metric_loop(
                metric,
                evc_client,
                vmc_client,
                output_dir,
                interval,
                max_parallel,
                top_n,
                stop_event,
                once,
            )
        )
        for metric in metrics
    ]
    await asyncio.gather(*tasks)
