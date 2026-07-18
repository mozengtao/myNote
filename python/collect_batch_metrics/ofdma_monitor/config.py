"""Application configuration, populated from argparse in ``cli.py``.

Kept as a plain dataclass (rather than parsing argparse.Namespace directly
throughout the codebase) so every other module has a single, typed, testable
object to depend on.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass
class AppConfig:
    output_dir: Path
    metrics: tuple[str, ...] = ("mer", "uncorrectable")
    interval: int = 600
    max_parallel: int = 12
    top_n: int = 100

    # ofdma-sub-carrier-mer metric
    mer_threshold_db: float = 40.0

    # xmit-chan-counter uncorrectable metric
    uncorrectable_channels: tuple[int, ...] = (12, 13)

    # nomad / CLI session settings
    cli_timeout: float = 60.0
    nomad_bin: str = "nomad"
    evc_task: str = "evc"
    evc_job: str = "evc"
    evc_user: str = "admin"
    vmc_task: str = "vmc"
    vmc_user: str = "admin"

    # logging
    log_level: str = "INFO"
    log_file: str | None = None

    # run a single round then exit (handy for manual testing)
    once: bool = False
