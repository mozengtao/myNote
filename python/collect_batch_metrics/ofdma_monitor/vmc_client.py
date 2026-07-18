"""Python replacement for ``vmccli.sh``.

Runs commands inside a specific VMC's ``confd_cli`` session (task ``vmc``,
job = the VMC's Nomad job name).
"""

from __future__ import annotations

import logging
from typing import Any

from .json_utils import extract_json_object
from .nomad_client import NomadCliRunner, NomadExecTarget

logger = logging.getLogger(__name__)

DEFAULT_VMC_TASK = "vmc"
DEFAULT_VMC_USER = "admin"

# Same assumption as evc_client.VMC_STATUS_COMMAND: `| display json` is used
# in place of the current `| tab | nomore`. Verify against a real VMC.
MODEM_BRIEF_COMMAND = (
    "show ccap docsis docs-mac-domain mac-domain modem brief | display json"
)


class VmcCliClient:
    """Equivalent of ``./vmccli.sh "<job>" "<command>"``."""

    def __init__(
        self,
        runner: NomadCliRunner,
        task: str = DEFAULT_VMC_TASK,
        user: str = DEFAULT_VMC_USER,
    ) -> None:
        self._runner = runner
        self._task = task
        self._exec_args = ["ip", "vrf", "exec", "podman", "confd_cli", "-u", user]

    async def run_command(self, job: str, command: str) -> str:
        """Return raw (possibly noisy) stdout for `command` run against `job`."""

        target = NomadExecTarget(task=self._task, job=job)
        return await self._runner.run_async(target, self._exec_args, [command])

    async def run_command_json(self, job: str, command: str) -> dict[str, Any]:
        raw = await self.run_command(job, command)
        return extract_json_object(raw)

    async def get_modem_brief(self, job: str) -> dict[str, Any]:
        """Replacement for `gen_vmc_cm_macs`'s CLI call (before awk filtering)."""

        return await self.run_command_json(job, MODEM_BRIEF_COMMAND)

    async def get_metric_data(self, job: str, command: str) -> dict[str, Any]:
        """Run a MetricPlugin-built per-CM command (e.g. ofdma-sub-carrier-mer,
        xmit-chan-counter) and return the extracted JSON payload.
        """

        return await self.run_command_json(job, command)
