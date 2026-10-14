"""`infrastructure.process_runner` 与 `services.command_service` /
`services.file_service` 的测试。

本文件刻意**不 mock 掉 subprocess 调用本身**：所有测试都针对机器上真实
存在的命令（`echo`、`sleep`、`ping` 等），用真实的子进程执行来验证超时、
输出截断、参数校验这些行为确实按预期工作——唯一被 mock 的地方是
"确认校验失败时底层根本没有尝试执行任何东西"这一个断言，这里需要用一个
假的 `process_runner.run_subprocess` 来证明它从未被调用过。

本文件不依赖 nicegui，因此可以在没有安装 nicegui 的机器上直接运行。
"""

from __future__ import annotations

import shutil
import sys
import time
from pathlib import Path
from unittest.mock import patch

import pytest

# 本项目没有 setup.py / pyproject.toml，也没有把 tests/ 做成一个带
# __init__.py 的包，pytest 默认只会把测试文件所在目录（tests/）加入
# sys.path，而不是项目根目录。这里显式把项目根目录加入 sys.path，确保
# `import services.xxx` / `import infrastructure.xxx` 在任何调用方式下
# （`pytest tests/test_command_service.py`、直接 `python3 tests/...py`、
# 在其他目录下调用）都能正常工作。
PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from infrastructure import process_runner  # noqa: E402
from services import command_service, file_service  # noqa: E402
from services.errors import (  # noqa: E402
    CommandTimeoutError,
    NotFoundError,
    PermissionDeniedError,
    ValidationError,
)

ECHO_PATH = shutil.which("echo")
SLEEP_PATH = shutil.which("sleep")


# ---------------------------------------------------------------------------
# infrastructure.process_runner
# ---------------------------------------------------------------------------


@pytest.mark.skipif(ECHO_PATH is None, reason="当前机器上找不到 echo 可执行文件")
def test_run_subprocess_success_returns_expected_stdout() -> None:
    """成功退出的命令：returncode 为 0，stdout 内容正确，未被截断。"""
    result = process_runner.run_subprocess(
        ECHO_PATH,
        ["hello-from-process-runner"],
        cwd=PROJECT_ROOT,
        timeout=5.0,
    )

    assert result.returncode == 0
    assert "hello-from-process-runner" in result.stdout
    assert result.stdout_truncated is False
    assert result.stderr_truncated is False
    assert result.duration_seconds >= 0.0


@pytest.mark.skipif(SLEEP_PATH is None, reason="当前机器上找不到 sleep 可执行文件")
def test_run_subprocess_timeout_kills_process_quickly() -> None:
    """超时场景：/bin/sleep 5 配合 timeout=0.5，必须真正被杀掉而不是傻等 5 秒。"""
    started_at = time.monotonic()

    with pytest.raises(CommandTimeoutError):
        process_runner.run_subprocess(
            SLEEP_PATH,
            ["5"],
            cwd=PROJECT_ROOT,
            timeout=0.5,
        )

    elapsed = time.monotonic() - started_at
    # 真正被杀掉的话，总耗时应该在 0.5 秒附近（留出调度/系统开销的余量），
    # 而不是接近 5 秒——如果断言失败说明超时根本没有生效，进程被傻等到了
    # 自然结束。
    assert elapsed < 2.0, f"耗时 {elapsed:.3f} 秒，超时机制可能没有真正生效"


def test_run_subprocess_nonexistent_executable_raises_execution_error() -> None:
    """可执行文件路径不存在（启动失败）必须转换为 CommandExecutionError。"""
    from services.errors import CommandExecutionError

    with pytest.raises(CommandExecutionError):
        process_runner.run_subprocess(
            "/definitely/not/a/real/executable-xyz",
            [],
            cwd=PROJECT_ROOT,
            timeout=5.0,
        )


# ---------------------------------------------------------------------------
# services.command_service
# ---------------------------------------------------------------------------


def test_command_whitelist_contains_expected_entries() -> None:
    """白名单必须正好是这四个约定好的命令名，且每条都解析了 executable 字段。"""
    assert set(command_service.COMMAND_WHITELIST.keys()) == {
        "list_directory",
        "show_disk_usage",
        "ping_once",
        "echo_text",
    }
    for spec in command_service.COMMAND_WHITELIST.values():
        # executable 可能是 None（当前机器上不可用），但字段本身必须存在。
        assert spec.executable is None or Path(spec.executable).is_absolute()


@pytest.mark.skipif(
    command_service.COMMAND_WHITELIST["echo_text"].executable is None,
    reason="echo_text 在当前机器上不可用",
)
def test_run_command_echo_text_success() -> None:
    """通过 command_service 执行一个会成功退出的白名单命令。"""
    result = command_service.run_command("echo_text", ["hello world"])

    assert result.command_name == "echo_text"
    assert result.process_result.returncode == 0
    assert "hello world" in result.process_result.stdout


def test_run_command_unknown_command_raises_permission_denied() -> None:
    """命令名不在白名单内 -> PermissionDeniedError。"""
    with pytest.raises(PermissionDeniedError):
        command_service.run_command("rm_everything", [])


def test_run_command_rejects_shell_injection_attempt_without_executing() -> None:
    """给 ping_once 传一个带 shell 特殊字符的主机名 -> ValidationError，
    并且确认底层 process_runner.run_subprocess 根本没有被调用过
    （校验在真正启动子进程之前就已经失败）。
    """
    call_recorder = {"called": False}

    def fake_run_subprocess(*args: object, **kwargs: object) -> None:
        call_recorder["called"] = True
        raise AssertionError(
            "不应该走到这里：参数校验必须在调用 process_runner.run_subprocess "
            "之前就失败"
        )

    with patch.object(command_service.process_runner, "run_subprocess", fake_run_subprocess):
        with pytest.raises(ValidationError):
            command_service.run_command("ping_once", ["; rm -rf /"])

    assert call_recorder["called"] is False, "校验失败后不应该尝试执行任何子进程"


def test_run_command_rejects_wrong_argument_count() -> None:
    """不需要参数的命令（show_disk_usage）如果被传了参数，也必须被拒绝。"""
    with pytest.raises(ValidationError):
        command_service.run_command("show_disk_usage", ["unexpected-arg"])


def test_run_command_rejects_directory_not_in_choice_list() -> None:
    """list_directory 的参数必须是预先登记的目录名之一，不能是任意路径。"""
    with pytest.raises(ValidationError):
        command_service.run_command("list_directory", ["/etc"])


@pytest.mark.skipif(
    command_service.COMMAND_WHITELIST["list_directory"].executable is None,
    reason="list_directory 在当前机器上不可用",
)
def test_run_command_list_directory_success() -> None:
    """list_directory 使用合法的目录选项时应当成功执行。"""
    result = command_service.run_command("list_directory", ["project_root"])

    assert result.process_result.returncode == 0
    # 项目根目录下一定有 requirements.txt，用它来验证确实列出了正确的目录。
    assert "requirements.txt" in result.process_result.stdout


# ---------------------------------------------------------------------------
# services.file_service
# ---------------------------------------------------------------------------


def test_list_viewable_files_filters_by_suffix() -> None:
    """只应该列出扩展名在白名单内的文件。"""
    names = file_service.list_viewable_files(PROJECT_ROOT, (".txt",))

    assert "requirements.txt" in names
    assert all(name.endswith(".txt") for name in names)


def test_read_text_file_rejects_parent_directory_escape() -> None:
    """`../requirements.txt` 试图逃逸出基准目录，必须被 PermissionDeniedError 拦下。"""
    sub_dir = PROJECT_ROOT / "examples"
    with pytest.raises(PermissionDeniedError):
        file_service.read_text_file("../requirements.txt", sub_dir)


def test_read_text_file_rejects_absolute_path() -> None:
    """绝对路径必须被 PermissionDeniedError 拦下。"""
    with pytest.raises(PermissionDeniedError):
        file_service.read_text_file("/etc/passwd", PROJECT_ROOT)


def test_read_text_file_rejects_disallowed_suffix() -> None:
    """扩展名不在白名单内的文件必须被 PermissionDeniedError 拦下。"""
    with pytest.raises(PermissionDeniedError):
        file_service.read_text_file(
            "requirements.txt", PROJECT_ROOT, allowed_suffixes=(".md",)
        )


def test_read_text_file_truncates_large_file() -> None:
    """用一个很小的 max_bytes 读取一个真实存在的文件，确认截断标记与长度限制。"""
    text, truncated = file_service.read_text_file(
        "requirements.txt", PROJECT_ROOT, max_bytes=50, allowed_suffixes=(".txt",)
    )

    assert truncated is True
    assert len(text) <= 50


def test_read_text_file_missing_file_raises_not_found() -> None:
    """不存在的文件名必须抛出 NotFoundError。"""
    with pytest.raises(NotFoundError):
        file_service.read_text_file("this-file-does-not-exist.md", PROJECT_ROOT)


if __name__ == "__main__":
    # 优先使用 `python3 -m pytest -q tests/test_command_service.py` 运行本文件；
    # 这个 `__main__` 入口是为"当前环境还没有安装 pytest"这种情况准备的兜底
    # 方案，手动依次调用每一个 test_ 开头的函数并汇报结果。
    import traceback

    test_functions = [
        obj
        for name, obj in sorted(globals().items())
        if name.startswith("test_") and callable(obj)
    ]

    passed = 0
    failed = 0
    for test_function in test_functions:
        try:
            test_function()
        except pytest.skip.Exception as exc:  # type: ignore[attr-defined]
            print(f"SKIP  {test_function.__name__}: {exc}")
            continue
        except Exception:  # noqa: BLE001 - 兜底脚本需要捕获并汇报所有异常
            print(f"FAIL  {test_function.__name__}")
            traceback.print_exc()
            failed += 1
        else:
            print(f"PASS  {test_function.__name__}")
            passed += 1

    print(f"\n{passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)
