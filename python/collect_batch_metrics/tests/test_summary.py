from ofdma_monitor.models import MetricSample
from ofdma_monitor.storage import RoundStorage
from ofdma_monitor.summary import summarize_round, update_common_cm_summary


def _sample(vmc: str, mac: str, count: int) -> MetricSample:
    return MetricSample(vmc_name=vmc, cm_mac=mac, values={"count": count}, rank_value=count)


def test_summarize_round_ranks_descending_and_limits_top_n(tmp_path):
    storage = RoundStorage(tmp_path, "mer")
    for idx, count in enumerate([5, 20, 1, 15]):
        storage.save_result(1, _sample("vmc-a", f"aa:bb:cc:dd:ee:0{idx}", count))

    top_rows = summarize_round(storage, round_no=1, top_n=2, columns=("count",))

    assert [row.values["count"] for row in top_rows] == [20, 15]
    summary_json = storage.round_dir(1) / "summary_top2.json"
    summary_txt = storage.round_dir(1) / "summary_top2.txt"
    assert summary_json.exists()
    assert summary_txt.exists()
    assert "VMC" in summary_txt.read_text(encoding="utf-8")


def test_summarize_round_returns_empty_when_no_significant_results(tmp_path):
    storage = RoundStorage(tmp_path, "mer")

    top_rows = summarize_round(storage, round_no=1, top_n=100, columns=("count",))

    assert top_rows == []


def test_update_common_cm_summary_only_keeps_cms_present_in_every_round(tmp_path):
    storage = RoundStorage(tmp_path, "mer")

    storage.save_result(1, _sample("vmc-a", "aa:aa:aa:aa:aa:aa", 10))
    storage.save_result(1, _sample("vmc-a", "bb:bb:bb:bb:bb:bb", 5))
    summarize_round(storage, round_no=1, top_n=100, columns=("count",))

    storage.save_result(2, _sample("vmc-a", "aa:aa:aa:aa:aa:aa", 8))
    summarize_round(storage, round_no=2, top_n=100, columns=("count",))

    entries = update_common_cm_summary(
        storage, current_round=2, top_n=100, columns=("count",)
    )

    assert entries is not None
    assert len(entries) == 1
    assert entries[0].vmc_name == "vmc-a"
    assert entries[0].cm_mac == "aa:aa:aa:aa:aa:aa"
    assert entries[0].per_round_values == {1: {"count": 10}, 2: {"count": 8}}
    assert entries[0].last_rank_value == 8

    assert (storage.metric_dir / "common_cm.json").exists()
    assert (storage.metric_dir / "common_cm.txt").exists()


def test_update_common_cm_summary_returns_none_without_any_round_summary(tmp_path):
    storage = RoundStorage(tmp_path, "mer")

    assert (
        update_common_cm_summary(storage, current_round=3, top_n=100, columns=("count",))
        is None
    )
