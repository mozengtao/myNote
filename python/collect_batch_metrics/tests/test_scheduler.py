import asyncio

from ofdma_monitor.scheduler import run_all


class _FlakyEvcClient:
    """Fails the first call to `get_vmc_status`, succeeds on every call after
    that -- simulates one metric's startup VMC-list fetch failing (as seen
    against a real system) while the sibling metric's fetch succeeds.
    """

    def __init__(self) -> None:
        self.calls = 0

    async def get_vmc_status(self):
        self.calls += 1
        if self.calls == 1:
            raise RuntimeError("simulated EVC failure on first call")
        return {
            "data": {
                "vcm-vmc-mapper-svc:vmc-state": {
                    "vmc": [
                        {
                            "name": "vmc-a",
                            "status": {"container-name": "job-a", "ha-status": "-"},
                        }
                    ]
                }
            }
        }


class _FakeVmcClient:
    async def get_modem_brief(self, job: str):
        return {"data": {"ccap:ccap": {"docsis": {"docs-mac-domain": {"mac-domain": []}}}}}

    async def get_metric_data(self, job: str, command: str):
        return {}


class _FakeMetric:
    def __init__(self, name: str) -> None:
        self.name = name
        self.columns = ("count",)
        self.rank_column = "count"

    def build_command(self, cm_mac: str) -> str:
        return "show something"

    def parse(self, vmc_name, cm_mac, raw_json):
        return None

    def is_significant(self, sample) -> bool:
        return False


def test_run_all_does_not_crash_when_one_metric_fails_to_start(tmp_path):
    metrics = [_FakeMetric("metric-a"), _FakeMetric("metric-b")]
    evc_client = _FlakyEvcClient()

    # Must complete without raising: previously an uncaught exception from
    # one metric's initial VMC-list fetch would propagate through
    # asyncio.gather in run_all and kill every other metric's loop too.
    asyncio.run(
        run_all(
            metrics=metrics,
            evc_client=evc_client,
            vmc_client=_FakeVmcClient(),
            output_dir=tmp_path,
            interval=600,
            max_parallel=4,
            top_n=10,
            once=True,
        )
    )

    vmc_list_written = [
        (tmp_path / metric.name / "vmc_name_job.json").exists() for metric in metrics
    ]
    # Exactly one metric's startup call hit the simulated failure; the other
    # ran its round to completion.
    assert sum(vmc_list_written) == 1
