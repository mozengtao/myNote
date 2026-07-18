"""Python replacement for ``evccli.sh``.

Runs commands inside the EVC's ``ncs_cli`` session (task/job both ``evc``),
sending ``unhide debug`` first, exactly like ``config.sh``/``evccli.sh`` do.
"""

from __future__ import annotations

import logging
from typing import Any

from .json_utils import extract_json_object
from .nomad_client import NomadCliRunner, NomadExecTarget

logger = logging.getLogger(__name__)

DEFAULT_EVC_TASK = "evc"
DEFAULT_EVC_JOB = "evc"
DEFAULT_EVC_USER = "admin"

# Assumption to verify against a real system: the ConfD/NCS CLI JSON pipe is
# `| display json`, mirroring the existing `| display xml` usage. See
# docs/ARCHITECTURE.md "known assumptions" for details.
VMC_STATUS_COMMAND = "show vmc status | display json"


class EvcCliClient:
    """Equivalent of ``./evccli.sh '<command>'``."""

    def __init__(
        self,
        runner: NomadCliRunner,
        task: str = DEFAULT_EVC_TASK,
        job: str = DEFAULT_EVC_JOB,
        user: str = DEFAULT_EVC_USER,
    ) -> None:
        self._runner = runner
        self._target = NomadExecTarget(task=task, job=job)
        self._exec_args = ["ncs_cli", "-u", user]

    async def run_command(self, command: str) -> str:
        """Send `unhide debug` then `command`; return raw (possibly noisy) stdout."""

        return await self._runner.run_async(
            self._target, self._exec_args, ["unhide debug", command]
        )

    async def run_command_json(self, command: str) -> dict[str, Any]:
        raw = await self.run_command(command)
        return extract_json_object(raw)

    async def get_vmc_status(self) -> dict[str, Any]:
        """Replacement for `gen_vmc_name_job`'s CLI call (before awk filtering)."""

        return await self.run_command_json(VMC_STATUS_COMMAND)
