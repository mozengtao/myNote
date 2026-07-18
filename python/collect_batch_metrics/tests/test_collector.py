import asyncio

import pytest

from ofdma_monitor.collector import _collect_one_cm, _collect_one_vmc
from ofdma_monitor.json_utils import JsonExtractionError
from ofdma_monitor.models import MetricSample, VmcInfo
from ofdma_monitor.nomad_client import NomadExecError
from ofdma_monitor.storage import RoundStorage


class _FakeMetric:
    name = "fake-metric"
    columns = ("count",)
    rank_column = "count"

    def build_command(self, cm_mac: str) -> str:
        return f"show {cm_mac}"

    def parse(self, vmc_name, cm_mac, raw_json):
        return MetricSample(vmc_name=vmc_name, cm_mac=cm_mac, values={"count": 1}, rank_value=1)

    def is_significant(self, sample) -> bool:
        return True


class _RaisingVmcClient:
    """Stand-in for VmcCliClient that always raises the given exception --
    simulates a flaky `nomad alloc exec` call or an empty/unparseable
    response (the exact failure mode reported against a real system: a
    JsonExtractionError from an occasional empty stdout under concurrent
    load, which previously crashed the whole scheduler process).
    """

    def __init__(self, exc: Exception) -> None:
        self._exc = exc

    async def get_metric_data(self, job: str, command: str):
        raise self._exc

    async def get_modem_brief(self, job: str):
        raise self._exc


@pytest.mark.parametrize("exc", [NomadExecError("boom"), JsonExtractionError("empty output")])
def test_collect_one_cm_does_not_raise_on_cli_errors(tmp_path, exc):
    storage = RoundStorage(tmp_path, "fake-metric")
    vmc = VmcInfo(name="vmc-a", job="job-a")
    semaphore = asyncio.Semaphore(1)

    # Must complete without raising -- this reproduces the crash reported
    # against a real system (an unhandled JsonExtractionError killed the
    # whole `python -m ofdma_monitor run` process).
    asyncio.run(
        _collect_one_cm(
            _RaisingVmcClient(exc),
            _FakeMetric(),
            storage,
            1,
            vmc,
            "aa:bb:cc:dd:ee:ff",
            semaphore,
        )
    )

    round_dir = storage.round_dir(1)
    if round_dir.exists():
        assert list(round_dir.glob("**/*.raw.json")) == []
        assert list(round_dir.glob("**/*.result.json")) == []


@pytest.mark.parametrize("exc", [NomadExecError("boom"), JsonExtractionError("empty output")])
def test_collect_one_vmc_does_not_raise_on_cli_errors(tmp_path, exc):
    storage = RoundStorage(tmp_path, "fake-metric")
    vmc = VmcInfo(name="vmc-a", job="job-a")
    semaphore = asyncio.Semaphore(1)

    tasks = asyncio.run(
        _collect_one_vmc(_RaisingVmcClient(exc), _FakeMetric(), storage, 1, vmc, semaphore)
    )

    assert tasks == []
