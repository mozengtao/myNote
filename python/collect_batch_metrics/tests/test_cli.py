from pathlib import Path

import pytest

from ofdma_monitor.cli import build_arg_parser, config_from_args


def test_run_subcommand_defaults():
    parser = build_arg_parser()
    args = parser.parse_args(["run"])
    config = config_from_args(args)

    assert config.output_dir == Path("output")
    assert config.metrics == ("mer", "uncorrectable")
    assert config.interval == 600
    assert config.max_parallel == 12
    assert config.top_n == 100
    assert config.uncorrectable_channels == (12, 13)


def test_run_subcommand_rejects_unknown_metric():
    parser = build_arg_parser()
    args = parser.parse_args(["run", "--metrics", "mer,bogus"])
    with pytest.raises(SystemExit):
        config_from_args(args)


def test_run_subcommand_parses_custom_channels_and_metrics():
    parser = build_arg_parser()
    args = parser.parse_args(
        ["run", "--metrics", "uncorrectable", "--uncorrectable-channels", "1,2,3"]
    )
    config = config_from_args(args)

    assert config.metrics == ("uncorrectable",)
    assert config.uncorrectable_channels == (1, 2, 3)


def test_debug_evc_subcommand_parses_command_and_connection_args():
    parser = build_arg_parser()
    args = parser.parse_args(
        ["debug-evc", "show vmc status | display json", "--evc-job", "evc"]
    )

    assert args.subcommand == "debug-evc"
    assert args.command == "show vmc status | display json"
    assert args.evc_job == "evc"
    assert args.cli_timeout == 60.0


def test_debug_vmc_subcommand_parses_job_and_command():
    parser = build_arg_parser()
    args = parser.parse_args(
        ["debug-vmc", "vmc-morris-dentist-1", "show ccap docsis docs-mac-domain mac-domain modem brief | display json"]
    )

    assert args.subcommand == "debug-vmc"
    assert args.job == "vmc-morris-dentist-1"
    assert "modem brief" in args.command
