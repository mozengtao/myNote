"""``argparse`` entry point: ``python -m ofdma_monitor run ...``.

This module is the *only* place that wires concrete implementations
together (the Nomad CLI runner, the EVC/VMC clients, and the registered
:class:`~ofdma_monitor.metrics.base.MetricPlugin` instances) -- every other
module only depends on protocols/interfaces.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
from collections.abc import Callable
from pathlib import Path

from .config import AppConfig
from .evc_client import EvcCliClient
from .json_utils import JsonExtractionError, extract_json_object
from .logging_setup import setup_logging
from .metrics.base import MetricPlugin
from .metrics.ofdma_mer import OfdmaMerMetric
from .metrics.xmit_uncorrectable import XmitUncorrectableMetric
from .nomad_client import NomadCliRunner
from .scheduler import run_all
from .vmc_client import VmcCliClient

logger = logging.getLogger(__name__)

#: Registry mapping a `--metrics` name to a factory that builds the plugin
#: from the parsed AppConfig. To add a new monitor: write `metrics/xxx.py`
#: implementing MetricPlugin, then add one entry here -- no other module
#: needs to change. See docs/ARCHITECTURE.md.
METRIC_FACTORIES: dict[str, Callable[[AppConfig], MetricPlugin]] = {
    "mer": lambda cfg: OfdmaMerMetric(threshold_db=cfg.mer_threshold_db),
    "uncorrectable": lambda cfg: XmitUncorrectableMetric(
        channels=cfg.uncorrectable_channels
    ),
}


def _parse_channels(value: str) -> tuple[int, ...]:
    return tuple(int(part) for part in value.split(",") if part.strip())


def _add_nomad_connection_args(parser: argparse.ArgumentParser) -> None:
    """Shared `nomad alloc exec` / CLI session flags, used by `run` and by
    both `debug-*` subcommands so they always agree on defaults.
    """

    parser.add_argument(
        "--cli-timeout",
        type=float,
        default=60.0,
        help="timeout in seconds for each `nomad alloc exec` call (default: 60)",
    )
    parser.add_argument("--nomad-bin", default="nomad", help="nomad binary to invoke")
    parser.add_argument("--evc-task", default="evc")
    parser.add_argument("--evc-job", default="evc")
    parser.add_argument("--evc-user", default="admin")
    parser.add_argument("--vmc-task", default="vmc")
    parser.add_argument("--vmc-user", default="admin")
    parser.add_argument(
        "--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"]
    )


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ofdma_monitor",
        description=(
            "Python reimplementation of monitor_csl_ofdma_mer.sh / "
            "monitor_csl_ofdma_uncorrectable.sh / run_ofdma_monitors.sh."
        ),
    )
    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    run_parser = subparsers.add_parser(
        "run", help="run the periodic monitor(s) until interrupted"
    )
    run_parser.add_argument(
        "--metrics",
        default="mer,uncorrectable",
        help=(
            "comma separated subset of "
            f"{sorted(METRIC_FACTORIES)} to run (default: all of them)"
        ),
    )
    run_parser.add_argument(
        "--output-dir", default="output", help="output root directory (default: output)"
    )
    run_parser.add_argument(
        "--interval", type=int, default=600, help="seconds between rounds (default: 600)"
    )
    run_parser.add_argument(
        "--max-parallel",
        type=int,
        default=12,
        help="max concurrent `nomad alloc exec` calls (default: 12)",
    )
    run_parser.add_argument(
        "--top-n", type=int, default=100, help="Top-N size for per-round summaries (default: 100)"
    )
    run_parser.add_argument(
        "--mer-threshold-db",
        type=float,
        default=40.0,
        help="RxMER (dB) threshold below which a sub-carrier counts as bad (default: 40.0)",
    )
    run_parser.add_argument(
        "--uncorrectable-channels",
        type=_parse_channels,
        default=(12, 13),
        help="comma separated OFDMA channel numbers to sum uncorrectable counters for (default: 12,13)",
    )
    _add_nomad_connection_args(run_parser)
    run_parser.add_argument(
        "--log-file", default=None, help="optional path to also log to (rotating file handler)"
    )
    run_parser.add_argument(
        "--once",
        action="store_true",
        help="run a single round per metric then exit (useful for manual testing)",
    )

    debug_evc_parser = subparsers.add_parser(
        "debug-evc",
        help=(
            "run one ad-hoc command against the EVC's ncs_cli session and print "
            "both the raw CLI output and the extracted JSON -- use this to "
            "validate the '| display json' assumption / schema before relying "
            "on `run`. Equivalent to `./evccli.sh '<command>'` but goes through "
            "the same code path as `run` (including JSON extraction)."
        ),
    )
    debug_evc_parser.add_argument(
        "command",
        help="ncs_cli command to run, e.g. 'show vmc status | display json'",
    )
    _add_nomad_connection_args(debug_evc_parser)

    debug_vmc_parser = subparsers.add_parser(
        "debug-vmc",
        help=(
            "run one ad-hoc command against a specific VMC's confd_cli session "
            "and print both the raw CLI output and the extracted JSON. "
            "Equivalent to `./vmccli.sh '<job>' '<command>'`."
        ),
    )
    debug_vmc_parser.add_argument("job", help="Nomad job name of the VMC, e.g. vmc-morris-dentist-1")
    debug_vmc_parser.add_argument(
        "command",
        help="confd_cli command to run, e.g. "
        "'show ccap docsis docs-mac-domain mac-domain modem brief | display json'",
    )
    _add_nomad_connection_args(debug_vmc_parser)

    return parser


def config_from_args(args: argparse.Namespace) -> AppConfig:
    metrics = tuple(m.strip() for m in args.metrics.split(",") if m.strip())
    unknown = sorted(set(metrics) - set(METRIC_FACTORIES))
    if unknown:
        raise SystemExit(
            f"unknown metric(s): {unknown}; available: {sorted(METRIC_FACTORIES)}"
        )
    if not metrics:
        raise SystemExit("--metrics must name at least one metric")

    return AppConfig(
        output_dir=Path(args.output_dir),
        metrics=metrics,
        interval=args.interval,
        max_parallel=args.max_parallel,
        top_n=args.top_n,
        mer_threshold_db=args.mer_threshold_db,
        uncorrectable_channels=args.uncorrectable_channels,
        cli_timeout=args.cli_timeout,
        nomad_bin=args.nomad_bin,
        evc_task=args.evc_task,
        evc_job=args.evc_job,
        evc_user=args.evc_user,
        vmc_task=args.vmc_task,
        vmc_user=args.vmc_user,
        log_level=args.log_level,
        log_file=args.log_file,
        once=args.once,
    )


def run_command(config: AppConfig) -> None:
    setup_logging(config.log_level, config.log_file)
    logger.info(
        "starting: metrics=%s output_dir=%s interval=%ds max_parallel=%d top_n=%d",
        config.metrics,
        config.output_dir,
        config.interval,
        config.max_parallel,
        config.top_n,
    )

    runner = NomadCliRunner(timeout=config.cli_timeout, nomad_bin=config.nomad_bin)
    evc_client = EvcCliClient(
        runner, task=config.evc_task, job=config.evc_job, user=config.evc_user
    )
    vmc_client = VmcCliClient(runner, task=config.vmc_task, user=config.vmc_user)

    metrics = [METRIC_FACTORIES[name](config) for name in config.metrics]

    asyncio.run(
        run_all(
            metrics=metrics,
            evc_client=evc_client,
            vmc_client=vmc_client,
            output_dir=config.output_dir,
            interval=config.interval,
            max_parallel=config.max_parallel,
            top_n=config.top_n,
            once=config.once,
        )
    )


def _print_raw_and_extracted_json(raw: str) -> None:
    """Prints the raw CLI stdout verbatim, then whatever
    `json_utils.extract_json_object` manages to pull out of it. Comparing the
    two makes it obvious whether a problem is (a) the CLI command itself
    returning something unexpected, (b) noise around the JSON that
    `extract_json_object` fails to strip, or (c) the JSON payload not
    matching the schema `parsers/`/`metrics/` assume.
    """

    print("----- RAW CLI OUTPUT -----")
    print(raw)
    print("----- EXTRACTED JSON (via ofdma_monitor.json_utils.extract_json_object) -----")
    try:
        data = extract_json_object(raw)
    except JsonExtractionError as exc:
        print(f"(failed to extract a JSON object: {exc})")
        return
    print(json.dumps(data, indent=2, ensure_ascii=False))


def run_debug_evc(args: argparse.Namespace) -> None:
    """Implements `debug-evc`: run one command against the EVC's ncs_cli."""

    setup_logging(args.log_level, None)
    runner = NomadCliRunner(timeout=args.cli_timeout, nomad_bin=args.nomad_bin)
    evc_client = EvcCliClient(
        runner, task=args.evc_task, job=args.evc_job, user=args.evc_user
    )
    logger.info(
        "running on EVC (task=%s job=%s): %s", args.evc_task, args.evc_job, args.command
    )
    raw = asyncio.run(evc_client.run_command(args.command))
    _print_raw_and_extracted_json(raw)


def run_debug_vmc(args: argparse.Namespace) -> None:
    """Implements `debug-vmc`: run one command against a VMC's confd_cli."""

    setup_logging(args.log_level, None)
    runner = NomadCliRunner(timeout=args.cli_timeout, nomad_bin=args.nomad_bin)
    vmc_client = VmcCliClient(runner, task=args.vmc_task, user=args.vmc_user)
    logger.info("running on VMC job=%s: %s", args.job, args.command)
    raw = asyncio.run(vmc_client.run_command(args.job, args.command))
    _print_raw_and_extracted_json(raw)


def main(argv: list[str] | None = None) -> None:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if args.subcommand == "run":
        run_command(config_from_args(args))
    elif args.subcommand == "debug-evc":
        run_debug_evc(args)
    elif args.subcommand == "debug-vmc":
        run_debug_vmc(args)


if __name__ == "__main__":
    main()
