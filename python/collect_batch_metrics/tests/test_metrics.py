from ofdma_monitor.metrics.ofdma_mer import OfdmaMerMetric
from ofdma_monitor.metrics.xmit_uncorrectable import XmitUncorrectableMetric


def test_ofdma_mer_metric_parses_real_sample(load_json_fixture):
    data = load_json_fixture("cm_ofdma_mer.json")
    metric = OfdmaMerMetric(threshold_db=40.0)

    sample = metric.parse("vmc-morris-dentist-1", "14:7d:05:47:bf:20", data)

    assert sample is not None
    assert sample.vmc_name == "vmc-morris-dentist-1"
    assert sample.cm_mac == "14:7d:05:47:bf:20"
    assert sample.values == {"count": 1342}
    assert sample.rank_value == 1342
    assert metric.is_significant(sample) is True


def test_ofdma_mer_metric_returns_none_for_unknown_cm(load_json_fixture):
    data = load_json_fixture("cm_ofdma_mer.json")
    metric = OfdmaMerMetric()

    assert metric.parse("vmc-x", "aa:bb:cc:dd:ee:ff", data) is None


def test_ofdma_mer_metric_not_significant_when_count_is_zero():
    metric = OfdmaMerMetric()
    from ofdma_monitor.models import MetricSample

    zero_sample = MetricSample(vmc_name="v", cm_mac="m", values={"count": 0}, rank_value=0)
    assert metric.is_significant(zero_sample) is False


def test_ofdma_mer_build_command_targets_requested_mac():
    metric = OfdmaMerMetric()
    command = metric.build_command("14:7d:05:47:bf:20")
    assert "14:7d:05:47:bf:20" in command
    assert "ofdma-sub-carrier-mer" in command
    assert command.endswith("| display json")


def test_xmit_uncorrectable_metric_zero_case(load_json_fixture):
    data = load_json_fixture("cm_uncorr.json")
    metric = XmitUncorrectableMetric(channels=(12, 13))

    sample = metric.parse("vmc-morris-dentist-1", "14:7d:05:47:bf:20", data)

    assert sample is not None
    assert sample.values == {"ch12_uncorr": 0, "ch13_uncorr": 0, "total": 0}
    assert metric.is_significant(sample) is False


def test_xmit_uncorrectable_metric_significant_case(load_json_fixture):
    data = load_json_fixture("cm_uncorr_significant.json")
    metric = XmitUncorrectableMetric(channels=(12, 13))

    sample = metric.parse("vmc-morris-dentist-1", "e8:3e:fc:30:94:15", data)

    assert sample is not None
    assert sample.values == {"ch12_uncorr": 7, "ch13_uncorr": 5, "total": 12}
    assert sample.rank_value == 12
    assert metric.is_significant(sample) is True


def test_xmit_uncorrectable_metric_is_configurable_for_other_channels(load_json_fixture):
    # cm_uncorr.json's real sample has a non-zero "corrected" (but zero
    # "uncorrectable") entry at ucid 1/12; confirm channel selection matters.
    data = load_json_fixture("cm_uncorr.json")
    metric = XmitUncorrectableMetric(channels=(0, 1))

    sample = metric.parse("vmc-morris-dentist-1", "14:7d:05:47:bf:20", data)

    assert sample is not None
    assert set(sample.values) == {"ch0_uncorr", "ch1_uncorr", "total"}
