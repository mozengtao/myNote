"""`services/task_query_service.py` 的单元测试。

本文件既可以被 `pytest` 收集运行，也可以直接用
`python3 tests/test_task_query_service.py` 执行——文件末尾的
`if __name__ == '__main__':` 会按顺序调用本模块里所有 `test_` 开头的函数，
并打印简单的通过/失败汇总。

运行方式（任选其一，均需要在项目根目录下执行）：
    python3 -m pytest -q tests/test_task_query_service.py
    python3 tests/test_task_query_service.py
"""

from __future__ import annotations

import sys
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from models.task import TaskStatus  # noqa: E402
from services.errors import ValidationError  # noqa: E402
from services.task_query_service import generate_mock_tasks, list_tasks  # noqa: E402


def test_generate_mock_tasks_is_reproducible_for_same_seed():
    """相同 seed 两次生成的结果必须逐字段一致（可复现）。"""
    first_batch = generate_mock_tasks(50, seed=42)
    second_batch = generate_mock_tasks(50, seed=42)

    assert len(first_batch) == len(second_batch) == 50
    for first, second in zip(first_batch, second_batch):
        assert first.id == second.id
        assert first.name == second.name
        assert first.status == second.status
        assert first.created_at == second.created_at
        assert first.logs == second.logs
        assert first.result == second.result
        assert first.error == second.error


def test_generate_mock_tasks_different_seed_can_differ():
    """不同 seed 应当（极大概率）产出不同的状态分布，用来确认 seed 确实生效。"""
    batch_a = generate_mock_tasks(50, seed=1)
    batch_b = generate_mock_tasks(50, seed=2)

    statuses_a = [task.status for task in batch_a]
    statuses_b = [task.status for task in batch_b]
    assert statuses_a != statuses_b


def test_generate_mock_tasks_all_marked_as_simulated():
    """所有生成的任务都必须标注为模拟数据，不能让人误以为是真实记录。"""
    tasks = generate_mock_tasks(20, seed=42)
    assert all(task.is_simulated is True for task in tasks)


def test_generate_mock_tasks_created_at_is_increasing():
    """created_at 应当随生成顺序依次递增。"""
    tasks = generate_mock_tasks(20, seed=42)
    for earlier, later in zip(tasks, tasks[1:]):
        assert earlier.created_at < later.created_at


def test_list_tasks_basic_pagination():
    """基本分页：第一页应当返回前 page_size 条，总数等于数据源长度（无筛选时）。"""
    tasks = generate_mock_tasks(25, seed=42)

    page_items, total = list_tasks(tasks, page=1, page_size=10)

    assert total == 25
    assert len(page_items) == 10
    assert page_items[0].id == tasks[0].id
    assert page_items[-1].id == tasks[9].id


def test_list_tasks_last_page_is_partial():
    """最后一页不满一页时，应当只返回剩余的那几条，而不是报错或补齐。"""
    tasks = generate_mock_tasks(25, seed=42)

    page_items, total = list_tasks(tasks, page=3, page_size=10)

    assert total == 25
    assert len(page_items) == 5  # 25 条，每页 10 条，第 3 页只剩 5 条
    assert page_items[0].id == tasks[20].id
    assert page_items[-1].id == tasks[24].id


def test_list_tasks_page_out_of_range_returns_empty_list():
    """page 超出范围时不应抛异常，而是返回空列表 + 正确的总数。"""
    tasks = generate_mock_tasks(25, seed=42)

    page_items, total = list_tasks(tasks, page=99, page_size=10)

    assert page_items == []
    assert total == 25


def test_list_tasks_status_filter_narrows_results():
    """status_filter 应当只保留匹配状态的任务，且总数也要反映筛选后的结果。"""
    tasks = generate_mock_tasks(200, seed=42)

    all_matching = [task for task in tasks if task.status == TaskStatus.FAILED]
    page_items, total = list_tasks(
        tasks, page=1, page_size=1_000_000, status_filter=TaskStatus.FAILED
    )

    assert total == len(all_matching)
    assert all(task.status == TaskStatus.FAILED for task in page_items)
    assert [task.id for task in page_items] == [task.id for task in all_matching]


def test_list_tasks_status_filter_none_means_no_filtering():
    """status_filter 为 None 时，不应该过滤任何任务。"""
    tasks = generate_mock_tasks(30, seed=42)

    page_items, total = list_tasks(tasks, page=1, page_size=1_000_000, status_filter=None)

    assert total == 30
    assert len(page_items) == 30


def test_list_tasks_invalid_page_raises_validation_error():
    """page 小于 1 时必须抛出 ValidationError。"""
    tasks = generate_mock_tasks(5, seed=42)
    try:
        list_tasks(tasks, page=0, page_size=10)
    except ValidationError:
        pass
    else:
        raise AssertionError('应当抛出 ValidationError，但没有抛出任何异常')


def test_list_tasks_invalid_page_size_raises_validation_error():
    """page_size 小于 1 时必须抛出 ValidationError。"""
    tasks = generate_mock_tasks(5, seed=42)
    try:
        list_tasks(tasks, page=1, page_size=0)
    except ValidationError:
        pass
    else:
        raise AssertionError('应当抛出 ValidationError，但没有抛出任何异常')


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
