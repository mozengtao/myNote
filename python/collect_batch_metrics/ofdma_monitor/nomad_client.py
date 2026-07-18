"""Low level wrapper around ``nomad alloc exec`` shared by the EVC and VMC clients.

Both ``evccli.sh`` and ``vmccli.sh`` follow the same pattern: pipe one or more
commands into an interactive CLI (`ncs_cli` / `confd_cli`) running inside a
Nomad allocation, e.g.::

    { printf '%s\\n' "unhide debug"; printf '%s\\n' "$cmd"; } | \\
        nomad alloc exec -task evc -job evc ncs_cli -u admin

``NomadCliRunner`` reproduces this with ``subprocess.run`` and exposes an
``asyncio``-friendly wrapper (``run_async``) that offloads the blocking call
to a worker thread via ``asyncio.to_thread`` so callers can bound concurrency
with an ``asyncio.Semaphore`` instead of bash job control.
"""

from __future__ import annotations

import asyncio
import logging
import subprocess
from dataclasses import dataclass

logger = logging.getLogger(__name__)


class NomadExecError(RuntimeError):
    """Raised when `nomad alloc exec` fails, times out, or cannot be launched."""


@dataclass(frozen=True)
class NomadExecTarget:
    """Identifies the Nomad allocation to exec into."""

    task: str
    job: str


class NomadCliRunner:
    """Runs a batch of CLI commands inside a ``nomad alloc exec`` session."""

    def __init__(self, timeout: float = 60.0, nomad_bin: str = "nomad") -> None:
        self.timeout = timeout
        self.nomad_bin = nomad_bin

    def run(
        self,
        target: NomadExecTarget,
        exec_args: list[str],
        stdin_lines: list[str],
    ) -> str:
        """Run synchronously; returns raw stdout (may contain CLI prompt/echo noise)."""

        cmd = [
            self.nomad_bin,
            "alloc",
            "exec",
            "-task",
            target.task,
            "-job",
            target.job,
            *exec_args,
        ]
        stdin_text = "\n".join(stdin_lines) + "\n"
        logger.debug(
            "nomad exec task=%s job=%s exec_args=%s", target.task, target.job, exec_args
        )
        try:
            proc = subprocess.run(
                cmd,
                input=stdin_text,
                capture_output=True,
                text=True,
                timeout=self.timeout,
            )
        except subprocess.TimeoutExpired as exc:
            raise NomadExecError(
                f"nomad alloc exec timed out after {self.timeout}s "
                f"(task={target.task}, job={target.job})"
            ) from exc
        except OSError as exc:
            raise NomadExecError(
                f"failed to launch '{self.nomad_bin}': {exc}"
            ) from exc

        if proc.returncode != 0:
            raise NomadExecError(
                f"nomad alloc exec exited with {proc.returncode} "
                f"(task={target.task}, job={target.job}): {proc.stderr.strip()[:500]}"
            )
        return proc.stdout

    async def run_async(
        self,
        target: NomadExecTarget,
        exec_args: list[str],
        stdin_lines: list[str],
    ) -> str:
        """Async wrapper: offloads the blocking `subprocess.run` call to a thread.

        This is the seam that lets the whole collector use `asyncio` for
        concurrency control (via `asyncio.Semaphore` / `asyncio.gather`) while
        the actual external process invocation still goes through
        `subprocess.run` as required.
        """

        return await asyncio.to_thread(self.run, target, exec_args, stdin_lines)
