"""`services/config_service.py` 的单元测试。

本文件既可以被 `pytest` 收集运行，也可以直接用
`python3 tests/test_config_service.py` 执行——文件末尾的
`if __name__ == '__main__':` 会按顺序调用本模块里所有 `test_` 开头的函数，
并打印简单的通过/失败汇总，方便在还没有确认 pytest 可用的环境里验证。

运行方式（任选其一，均需要在项目根目录下执行）：
    python3 -m pytest -q tests/test_config_service.py
    python3 tests/test_config_service.py
"""

from __future__ import annotations

import sys
from pathlib import Path

# 保证无论从哪个目录、用哪种方式（pytest 还是直接运行脚本）执行本文件，
# 项目根目录都在 sys.path 上，这样 `from services... import ...` 才能找到包。
_PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from services.config_service import (  # noqa: E402
    load_json_config,
    resolve_config_path,
    validate_config,
)
from services.errors import NotFoundError, PermissionDeniedError, ValidationError  # noqa: E402

_CONFIG_DIR = _PROJECT_ROOT / 'config'
_REQUIRED_FIELDS = ['name', 'host', 'port', 'environment']


def test_resolve_and_load_valid_config_succeeds():
    """合法配置文件应当能被成功解析为字典，字段值与 config/example.json 一致。"""
    path = resolve_config_path('example.json', _CONFIG_DIR)
    data = load_json_config(path)

    assert data['name'] == 'edge-lab-01'
    assert data['host'] == '192.168.1.50'
    assert data['port'] == 22
    assert data['environment'] == 'staging'


def test_invalid_json_raises_validation_error_with_position():
    """config/example_invalid.json 末尾多了个逗号，必须触发 ValidationError，
    且错误信息里要带上行号/列号方便定位问题。
    """
    path = resolve_config_path('example_invalid.json', _CONFIG_DIR)

    try:
        load_json_config(path)
    except ValidationError as exc:
        message = str(exc)
        assert '行' in message and '列' in message
    else:
        raise AssertionError('应当抛出 ValidationError，但没有抛出任何异常')


def test_missing_file_raises_not_found_error():
    """请求一个不存在的文件名，必须抛出 NotFoundError。"""
    try:
        resolve_config_path('does_not_exist.json', _CONFIG_DIR)
    except NotFoundError:
        pass
    else:
        raise AssertionError('应当抛出 NotFoundError，但没有抛出任何异常')


def test_parent_traversal_raises_permission_denied_error():
    """用 `../requirements.txt` 尝试越出 config/ 目录，必须抛出 PermissionDeniedError。"""
    try:
        resolve_config_path('../requirements.txt', _CONFIG_DIR)
    except PermissionDeniedError:
        pass
    else:
        raise AssertionError('应当抛出 PermissionDeniedError，但没有抛出任何异常')


def test_absolute_path_raises_permission_denied_error():
    """用绝对路径尝试绕过基准目录限制，必须抛出 PermissionDeniedError。"""
    absolute_path = str((_PROJECT_ROOT / 'requirements.txt').resolve())
    try:
        resolve_config_path(absolute_path, _CONFIG_DIR)
    except PermissionDeniedError:
        pass
    else:
        raise AssertionError('应当抛出 PermissionDeniedError，但没有抛出任何异常')


def test_validate_config_reports_missing_fields():
    """缺少必需字段的配置，validate_config 必须返回非空的问题列表。"""
    incomplete = {'name': 'edge-lab-01'}
    errors = validate_config(incomplete, _REQUIRED_FIELDS)

    assert errors, '缺少字段的配置应当产生至少一条校验错误'
    assert any('host' in message for message in errors)
    assert any('port' in message for message in errors)
    assert any('environment' in message for message in errors)


def test_validate_config_passes_for_complete_config():
    """config/example.json 对应的合法配置，validate_config 必须返回空列表。"""
    path = resolve_config_path('example.json', _CONFIG_DIR)
    data = load_json_config(path)

    errors = validate_config(data, _REQUIRED_FIELDS)

    assert errors == []


def test_validate_config_flags_invalid_port_value():
    """port 字段类型/范围不合法时，即便字段"存在"，也应当被 validate_config 发现。"""
    bad_port_config = {
        'name': 'edge-lab-03',
        'host': '10.0.0.1',
        'port': 70000,
        'environment': 'production',
    }
    errors = validate_config(bad_port_config, _REQUIRED_FIELDS)

    assert any('port' in message for message in errors)


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
