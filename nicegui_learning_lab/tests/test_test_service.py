"""`services/test_service.py` 的单元测试。

本文件既可以被 `pytest` 收集运行，也可以直接用
`python3 tests/test_test_service.py` 执行——文件末尾的
`if __name__ == '__main__':` 会按顺序调用本模块里所有 `test_` 开头的函数
（用 `asyncio.run()` 驱动其中的协程测试），并打印简单的通过/失败汇总。

`services/test_service.py` 不 import nicegui，所以本文件也完全不依赖
nicegui 是否已安装。

运行方式（任选其一，均需要在项目根目录下执行）：
    python3 -m pytest -q tests/test_test_service.py
    python3 tests/test_test_service.py

本文件所有测试都把 `step_delay` 设置为一个很小的值（0.01 秒），确保整个
测试文件能在一秒左右跑完，而不是真的等待生产场景下的 0.3 秒/步。
"""

from __future__ import annotations

import asyncio
import sys
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from models.task import TaskStatus  # noqa: E402
from services.errors import ValidationError  # noqa: E402
from services.test_service import TEST_SUITE, run_test_suite  # noqa: E402

#: 测试里统一使用的"很小的模拟步骤耗时"，让 6 条用例也能在几十毫秒内跑完。
_FAST_STEP_DELAY = 0.01

#: 便于测试里引用：所有不包含 `simulate_failure_case` 的用例 id。
_NON_FAILING_IDS = [case.id for case in TEST_SUITE if case.id != 'simulate_failure_case']


def _make_recorder() -> tuple[list[float], list[str], callable, callable]:
    """构造一对简单的 `on_progress` / `on_log` 回调，并把调用记录保存下来。"""
    progress_calls: list[float] = []
    log_calls: list[str] = []

    def on_progress(value: float) -> None:
        progress_calls.append(value)

    def on_log(line: str) -> None:
        log_calls.append(line)

    return progress_calls, log_calls, on_progress, on_log


def test_run_test_suite_success_path_reaches_succeeded():
    """选择一个不包含 simulate_failure_case 的子集，应当正常跑完到 SUCCEEDED。"""

    async def _run():
        progress_calls, log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        selected = _NON_FAILING_IDS[:3]
        record = await run_test_suite(
            selected,
            on_progress=on_progress,
            on_log=on_log,
            cancel_event=cancel_event,
            step_delay=_FAST_STEP_DELAY,
        )

        assert record.status is TaskStatus.SUCCEEDED
        assert record.progress == 1.0
        assert record.is_simulated is True
        assert record.error is None
        assert record.result is not None
        assert record.started_at is not None
        assert record.finished_at is not None
        # 每完成一条用例应当触发一次 on_progress，且最后一次应当是 1.0。
        assert len(progress_calls) == len(selected)
        assert progress_calls[-1] == 1.0
        # on_log 和 record.logs 两边都要写，且内容应当一致。
        assert log_calls == record.logs

    asyncio.run(_run())


def test_run_test_suite_failure_case_marks_failed_and_sets_error():
    """选择包含 simulate_failure_case 的子集，应当以 FAILED 收尾且 error 非空。"""

    async def _run():
        progress_calls, log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        # 故意把 simulate_failure_case 放在 run_ping_sweep 之前：
        # run_test_suite() 按 selected_ids 传入的顺序执行（见其文档），
        # 所以这个顺序才能真正验证"失败后不再执行后面排队的用例"。
        selected = ['check_cm_registration', 'simulate_failure_case', 'run_ping_sweep']
        record = await run_test_suite(
            selected,
            on_progress=on_progress,
            on_log=on_log,
            cancel_event=cancel_event,
            step_delay=_FAST_STEP_DELAY,
        )

        assert record.status is TaskStatus.FAILED
        assert record.error
        assert record.is_simulated is True
        # simulate_failure_case 排在 check_cm_registration 之后、
        # run_ping_sweep 之前（TEST_SUITE 固定顺序），失败后不应该再继续执行
        # run_ping_sweep —— 日志里不应出现它的"开始执行"记录。
        ping_sweep_name = next(case.name for case in TEST_SUITE if case.id == 'run_ping_sweep')
        start_log_lines = [line for line in log_calls if line.startswith('[模拟] 开始执行用例')]
        assert not any(ping_sweep_name in line for line in start_log_lines)
        assert log_calls == record.logs

    asyncio.run(_run())


def test_run_test_suite_cancel_event_set_during_first_log_stops_early():
    """在第一条日志出现后立即 set() cancel_event，应当以 CANCELLED 收尾。"""

    async def _run():
        cancel_event = asyncio.Event()
        log_calls: list[str] = []

        def on_log(line: str) -> None:
            log_calls.append(line)
            # 第一条日志（"开始执行用例 xxx"）一出现就请求取消；
            # run_test_suite 只会在"下一个用例开始之前"的检查点感知到这个
            # 请求——这正是"协作式取消"的核心语义，不是立即中断。
            if len(log_calls) == 1:
                cancel_event.set()

        def on_progress(_value: float) -> None:
            pass

        selected = _NON_FAILING_IDS[:3]
        record = await run_test_suite(
            selected,
            on_progress=on_progress,
            on_log=on_log,
            cancel_event=cancel_event,
            step_delay=_FAST_STEP_DELAY,
        )

        assert record.status is TaskStatus.CANCELLED
        assert record.is_simulated is True
        assert record.finished_at is not None
        # 第一条用例已经开始执行并记了日志，但第二条用例永远不会开始。
        assert any('开始执行' in line for line in log_calls)

    asyncio.run(_run())


def test_run_test_suite_unknown_id_raises_validation_error():
    """selected_ids 中包含不存在的 id，必须抛出 ValidationError。"""

    async def _run():
        _progress_calls, _log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        try:
            await run_test_suite(
                ['check_cm_registration', 'this_id_does_not_exist'],
                on_progress=on_progress,
                on_log=on_log,
                cancel_event=cancel_event,
                step_delay=_FAST_STEP_DELAY,
            )
        except ValidationError:
            pass
        else:
            raise AssertionError('应当抛出 ValidationError，但没有抛出任何异常')

    asyncio.run(_run())


def test_run_test_suite_empty_selection_raises_validation_error():
    """selected_ids 为空列表时，必须抛出 ValidationError。"""

    async def _run():
        _progress_calls, _log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        try:
            await run_test_suite(
                [],
                on_progress=on_progress,
                on_log=on_log,
                cancel_event=cancel_event,
                step_delay=_FAST_STEP_DELAY,
            )
        except ValidationError:
            pass
        else:
            raise AssertionError('应当抛出 ValidationError，但没有抛出任何异常')

    asyncio.run(_run())


def test_run_test_suite_executes_in_selected_ids_order_not_test_suite_order():
    """执行顺序必须严格跟随 `selected_ids` 传入的顺序，而不是 `TEST_SUITE` 的展示顺序。"""

    async def _run():
        _progress_calls, log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        # 故意传入和 TEST_SUITE 展示顺序相反的 id 列表。
        reversed_ids = list(reversed(_NON_FAILING_IDS[:3]))
        record = await run_test_suite(
            reversed_ids,
            on_progress=on_progress,
            on_log=on_log,
            cancel_event=cancel_event,
            step_delay=_FAST_STEP_DELAY,
        )

        assert record.status is TaskStatus.SUCCEEDED
        start_log_sequence = [line for line in log_calls if line.startswith('[模拟] 开始执行用例')]
        assert len(start_log_sequence) == len(reversed_ids)
        for case_id, log_line in zip(reversed_ids, start_log_sequence):
            case_name = next(case.name for case in TEST_SUITE if case.id == case_id)
            assert case_name in log_line

    asyncio.run(_run())


def test_run_test_suite_is_fast_with_small_step_delay():
    """用很小的 step_delay 跑完全部 6 条用例（含失败用例提前结束），应当非常快。"""

    async def _run():
        import time

        _progress_calls, _log_calls, on_progress, on_log = _make_recorder()
        cancel_event = asyncio.Event()

        all_ids = [case.id for case in TEST_SUITE]
        start = time.monotonic()
        record = await run_test_suite(
            all_ids,
            on_progress=on_progress,
            on_log=on_log,
            cancel_event=cancel_event,
            step_delay=_FAST_STEP_DELAY,
        )
        duration = time.monotonic() - start

        # 6 条用例、每条最多 0.01 秒，总耗时应当远小于 1 秒；这里留足余量。
        assert duration < 1.0
        # 全量 TEST_SUITE 里包含 simulate_failure_case，应当以 FAILED 收尾。
        assert record.status is TaskStatus.FAILED

    asyncio.run(_run())


if __name__ == '__main__':
    _tests = [
        (name, obj)
        for name, obj in sorted(globals().items())
        if name.startswith('test_') and callable(obj)
    ]

    passed = 0
    failed = 0
    for name, test_func in _tests:
        try:
            test_func()
        except Exception as exc:  # noqa: BLE001 - 这里就是要捕获所有失败并汇总打印
            failed += 1
            print(f'[FAIL] {name}: {exc}')
        else:
            passed += 1
            print(f'[PASS] {name}')

    print(f'\n共 {len(_tests)} 个测试，通过 {passed} 个，失败 {failed} 个。')
    if failed:
        sys.exit(1)
