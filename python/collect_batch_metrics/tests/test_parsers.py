from ofdma_monitor.parsers.merdata_table import count_below_threshold
from ofdma_monitor.parsers.modem_brief import parse_modem_macs
from ofdma_monitor.parsers.vmc_status import (
    _job_from_container_name,
    is_included,
    parse_vmc_status,
)


def test_parse_modem_macs_deduplicates_and_preserves_order(load_json_fixture):
    data = load_json_fixture("cm_brief.json")
    assert parse_modem_macs(data) == [
        "a8:11:fc:ee:13:7e",
        "20:6a:94:49:66:08",
        "14:7d:05:47:bf:20",
        "e8:3e:fc:30:94:15",
    ]


def test_parse_vmc_status_returns_included_vmc(load_json_fixture):
    data = load_json_fixture("vmc_status.json")
    vmcs = parse_vmc_status(data)
    assert len(vmcs) == 1
    vmc = vmcs[0]
    assert vmc.name == "vmc-morris-dentist-1"
    # The Nomad job name is the *full* status.container-name value (UUID
    # suffix included), not the vmc.name field -- confirmed against a real
    # system: `nomad alloc exec -task vmc -job <container-name> sh` works,
    # while `-job vmc-morris-dentist-1` (vmc.name) fails with
    # "No job(s) with prefix or ID ... found".
    assert vmc.job == "vmc-evc-dentist-31432bd8-8ef9-45e3-97fb-dbdf108d4f11"
    assert vmc.state == "Running"
    assert vmc.internal_state == "Complete"
    assert vmc.ha_status == "-"


def test_is_included_filters_standby_graphite_and_hot():
    assert is_included({"name": "vmc-foo", "status": {"ha-status": "-"}}) is True
    assert is_included({"name": "vmc-foo-standby", "status": {"ha-status": "-"}}) is False
    assert is_included({"name": "graphite-vmc", "status": {"ha-status": "-"}}) is False
    assert is_included({"name": "vmc-foo", "status": {"ha-status": "hot"}}) is False


def test_job_from_container_name_uses_container_name_verbatim():
    container_name = "vmc-evc-dentist-31432bd8-8ef9-45e3-97fb-dbdf108d4f11"
    assert _job_from_container_name(container_name, fallback="unused") == container_name


def test_job_from_container_name_falls_back_when_empty():
    assert _job_from_container_name("", fallback="vmc-morris-dentist-1") == "vmc-morris-dentist-1"


def test_count_below_threshold_on_real_merdata_sample(load_json_fixture):
    data = load_json_fixture("cm_ofdma_mer.json")
    modem = data["data"]["ccap:ccap"]["docsis"]["docs-mac-domain"]["mac-domain"][1][
        "ccap-oper:modem"
    ][0]
    merdata = modem["ofdma-sub-carrier-mer"]["merdata"]
    # Cross-checked by independently re-running the same awk-equivalent logic
    # against the fixture; keep this in sync if the fixture ever changes.
    assert count_below_threshold(merdata, threshold_db=40.0) == 1342


def test_count_below_threshold_only_counts_current_values_before_old_marker():
    merdata = (
        "RxMER data (dB) for channel 1 sub-carrier[1 --> 2], avg 40 dB:\n"
        "1(1.00MHz): 10.0 50.0 old= 1.0 2.0\n"
    )
    assert count_below_threshold(merdata) == 1


def test_count_below_threshold_returns_zero_without_header():
    assert count_below_threshold("no RxMER header in this text") == 0
