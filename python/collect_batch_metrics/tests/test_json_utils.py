import pytest

from ofdma_monitor.json_utils import (
    JsonExtractionError,
    extract_json_object,
    iter_mac_domain_modems,
)


def test_extract_json_object_strips_leading_and_trailing_cli_noise():
    raw = (
        "admin@ncs# show vmc status | display json\n"
        '{"data": {"a": 1}}\n'
        "admin@ncs# "
    )
    assert extract_json_object(raw) == {"data": {"a": 1}}


def test_extract_json_object_skips_a_brace_that_does_not_parse():
    # The stray "{" from "{oops" must be skipped in favor of the real object.
    raw = '{oops\n{"data": {}}'
    assert extract_json_object(raw) == {"data": {}}


def test_extract_json_object_raises_when_no_json_present():
    with pytest.raises(JsonExtractionError):
        extract_json_object("no json here at all")


def test_iter_mac_domain_modems_over_cm_brief(load_json_fixture):
    data = load_json_fixture("cm_brief.json")
    macs = [mac for _domain, mac, _leaf in iter_mac_domain_modems(data, "brief")]
    assert macs == [
        "a8:11:fc:ee:13:7e",
        "20:6a:94:49:66:08",
        "14:7d:05:47:bf:20",
        "e8:3e:fc:30:94:15",
    ]


def test_iter_mac_domain_modems_skips_mac_domain_without_modems(load_json_fixture):
    # cm_ofdma_mer.json's first mac-domain has no "ccap-oper:modem" key at all.
    data = load_json_fixture("cm_ofdma_mer.json")
    results = list(iter_mac_domain_modems(data, "ofdma-sub-carrier-mer"))
    assert len(results) == 1
    domain, mac, leaf = results[0]
    assert domain == "morris-dentist-1_md1_1"
    assert mac == "14:7d:05:47:bf:20"
    assert "merdata" in leaf
