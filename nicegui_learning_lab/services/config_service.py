"""配置文件读取与校验服务。

为什么需要这个模块
------------------
示例 4（JSON 配置检查器）需要两类"把字符串变成可信数据"的能力：把一个文件名
安全地解析成磁盘上的真实路径、把文件内容解析成 Python 字典，并判断这个字典
是否满足业务上要求的字段。这三件事都与 NiceGUI 无关——它们是"给定输入，
产出结果或抛出已知异常"的纯业务逻辑，所以被放进 services/ 而不是直接写在
examples/04_config_checker.py 里。这样 UI 代码只需要调用这里的函数、捕获
已知的异常类型、把结果渲染成界面，完全不需要知道 JSON 要怎么解析、一份合法
配置应该长什么样。

两种错误处理风格
----------------
本模块里刻意混用了两种不同的错误处理风格，这是有意为之，而不是不一致：

- `load_json_config()` 遇到任何一种"无法继续"的问题（文件不存在、没有
  权限、JSON 语法本身就不合法）都会立即抛出异常。这类问题是"单点致命的"：
  只要有一个就无法产出任何可用的字典，继续执行没有意义，调用方也没有办法
  在同一次调用里"顺便"发现第二个同类问题。
- `validate_config()` 则完全不抛异常，而是把所有发现的问题收集进一个列表
  一次性返回。这是因为"配置内容是否符合业务规则"天然是可以有多个独立问题
  同时存在的（比如同时缺少 `host` 和 `port`），而且用户体验上，一次性把所有
  问题都展示出来，比每次只报告第一个、改一个报一个要好得多。

简单说：**解析阶段"要么成功要么失败"，用异常；校验阶段"可能有 0 到多个
问题"，用返回值。**
"""

from __future__ import annotations

import json
from pathlib import Path

from services.errors import NotFoundError, PermissionDeniedError, ValidationError
from services.path_guard import confine_path


def resolve_config_path(filename: str, base_dir: Path) -> Path:
    """把用户提供的配置文件名限制在 `base_dir` 内解析。

    这里直接复用 `services/path_guard.py` 的 `confine_path()`，不重新实现
    一遍越界检测逻辑——越界判断是一个容易出安全漏洞的地方，整个项目只应该
    有一份实现。

    Args:
        filename: 用户在界面上选择/输入的文件名（可带子目录，但不能是
            绝对路径，也不能通过 `..` 逃逸出 `base_dir`）。
        base_dir: 允许访问的基准目录，本项目中通常是 `config/` 目录。

    Returns:
        一个保证落在 `base_dir` 内部、且确实存在的绝对路径。

    Raises:
        PermissionDeniedError: `filename` 是绝对路径，或解析后逃逸出了
            `base_dir`。
        NotFoundError: 解析出的路径不存在。
    """
    return confine_path(filename, base_dir, must_exist=True)


def parse_json_text(text: str) -> dict:
    """把已经在内存里的原始文本解析为 JSON 字典。

    这是 `load_json_config()`（从磁盘文件读取）与"用户通过 `ui.upload`
    上传、内容只存在于内存/请求中"这两条路径共用的解析逻辑——两条路径的
    来源完全不同（一个要经过 `resolve_config_path()` 的目录越界检查，另一
    个根本不落盘、无需也不应该做路径检查），但"字符串 -> JSON 字典"这一步
    的规则应当完全一致，所以抽成一个公共函数，而不是在示例代码里重复一遍
    `json.loads()` + 错误格式化逻辑。

    Args:
        text: 原始文本内容（UTF-8 字符串）。

    Returns:
        解析出的 JSON 对象（`dict`）。

    Raises:
        ValidationError: 文本不是合法的 JSON，或最外层不是一个 JSON 对象；
            错误信息中包含 `json.JSONDecodeError` 给出的行号/列号。
    """
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValidationError(
            f'JSON 语法错误（第 {exc.lineno} 行，第 {exc.colno} 列）: {exc.msg}'
        ) from exc

    if not isinstance(data, dict):
        raise ValidationError(
            f'配置文件最外层必须是一个 JSON 对象（花括号 {{}}），实际是 {type(data).__name__}'
        )

    return data


def load_json_config(path: Path) -> dict:
    """读取 `path` 指向的文件并解析为 JSON 字典。

    Args:
        path: 一个已经确定要读取的文件路径（通常是 `resolve_config_path()`
            的返回值，但也可以是任意调用方自行确保安全的路径——这个函数
            本身不做目录越界检查，那是 `resolve_config_path()` 的职责）。

    Returns:
        解析出的 JSON 对象（`dict`）。

    Raises:
        NotFoundError: 文件不存在。
        PermissionDeniedError: 操作系统拒绝了读取请求（权限不足）。
        ValidationError: 文件内容不是合法的 JSON；错误信息中包含
            `json.JSONDecodeError` 给出的行号/列号，方便用户定位问题。
    """
    try:
        text = path.read_text(encoding='utf-8')
    except FileNotFoundError as exc:
        raise NotFoundError(f'文件不存在: {path}') from exc
    except PermissionError as exc:
        raise PermissionDeniedError(f'没有权限读取文件: {path}') from exc

    return parse_json_text(text)


def validate_config(data: dict, required_fields: list[str]) -> list[str]:
    """校验配置字典是否满足业务规则，返回所有发现的问题。

    这个函数故意不抛异常（见模块文档顶部"两种错误处理风格"一节）：调用方
    应当把返回的列表直接展示给用户，空列表代表校验通过。

    Args:
        data: 已经成功解析出的配置字典（通常来自 `load_json_config()`）。
        required_fields: 必须存在、且取值非空的字段名列表。

    Returns:
        校验失败原因组成的字符串列表；为空代表没有发现任何问题。
    """
    errors: list[str] = []

    for field_name in required_fields:
        if field_name not in data:
            errors.append(f"缺少必需字段: '{field_name}'")
            continue
        value = data[field_name]
        if value is None or (isinstance(value, str) and not value.strip()):
            errors.append(f"字段 '{field_name}' 不能为空")

    # 下面是针对本项目示例配置里几个常见字段的轻量级语义校验：
    # 只有字段确实存在时才检查，不会因为字段本来就不要求存在而报错。
    if 'port' in data:
        port = data['port']
        if not isinstance(port, int) or isinstance(port, bool):
            errors.append(f"字段 'port' 应为整数，实际类型为 {type(port).__name__}")
        elif not (0 < port < 65536):
            errors.append(f"字段 'port' 超出合法端口范围 (1-65535): {port}")

    if 'timeout_seconds' in data:
        timeout = data['timeout_seconds']
        if not isinstance(timeout, (int, float)) or isinstance(timeout, bool) or timeout <= 0:
            errors.append(f"字段 'timeout_seconds' 应为正数，实际为 {timeout!r}")

    return errors
