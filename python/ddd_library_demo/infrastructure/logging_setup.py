"""
Infrastructure / Logging setup

Why logging configuration lives in Infrastructure
------------------------------------------------------
Deciding the log format, level, and output stream is an operational
concern, not a domain one. Domain/Application code only ever calls
`logging.getLogger(__name__)` and logs at an appropriate level; it never
configures handlers or formatters itself, so this one function is the
single place that decides how those log records actually get displayed.
"""

from __future__ import annotations

import logging


def configure_logging(level: int = logging.INFO) -> None:
    logging.basicConfig(
        level=level,
        format="%(levelname)s %(name)s: %(message)s",
    )
