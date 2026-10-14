"""共享的路径安全防护工具。

为什么需要这个模块
------------------
项目里至少有两类场景需要"用户/界面提供一个文件名，但只允许访问某个指定
目录之内的文件"：示例 4 的配置文件检查器（只能读 `config/` 目录下的文件）、
示例 6 的受控文件查看面板（只能读项目内某个指定目录下的文件）。这两类场景
背后是同一个安全规则，所以把它抽成一个共享函数，而不是在 `config_service`
和 `file_service` 里各写一份、容易出现"一边改了另一边忘了改"的不一致。

心智模型
--------
`confine_path(filename, base_dir)` 回答的问题是："把 `filename` 和
`base_dir` 拼在一起、并解析掉所有 `..`、符号链接之后，结果是否仍然落在
`base_dir` 内部？" 只要答案是"否"，就一律当成一次越权访问尝试，抛出
`PermissionDeniedError`，绝不把拼接后的路径交给调用方——哪怕 `filename`
长得完全不像一次攻击（例如只是不小心传入了一个绝对路径）。

本模块只依赖 `services/errors.py` 和标准库，不依赖 nicegui。
"""

from __future__ import annotations

from pathlib import Path

from services.errors import NotFoundError, PermissionDeniedError


def confine_path(filename: str, base_dir: Path, *, must_exist: bool = True) -> Path:
    """将 `filename` 限制在 `base_dir` 内解析，拒绝任何越界访问。

    Args:
        filename: 用户或界面提供的文件名（可以带子目录，但不能是绝对路径）。
        base_dir: 允许访问的基准目录，调用方需保证它本身是一个存在的目录。
        must_exist: 为 True 时，如果解析出的路径不存在会抛出 NotFoundError。

    Returns:
        一个保证落在 `base_dir` 内部的绝对路径。

    Raises:
        PermissionDeniedError: `filename` 是绝对路径，或解析后的路径
            （含 `..`、符号链接解析之后）逃逸出了 `base_dir`。
        NotFoundError: `must_exist` 为 True 且解析出的路径不存在。
    """
    # pathlib 的一个常见陷阱：`Path('/a') / '/etc/passwd'` 会直接返回
    # `/etc/passwd`（绝对路径会整体替换掉左边的路径），所以必须在拼接之前
    # 就把绝对路径挡掉，不能指望后面的 relative_to() 检查能兜住这种情况。
    if Path(filename).is_absolute():
        raise PermissionDeniedError(f"不允许使用绝对路径: {filename!r}")

    base = base_dir.resolve()
    candidate = (base / filename).resolve()

    try:
        candidate.relative_to(base)
    except ValueError as exc:
        raise PermissionDeniedError(
            f"路径越界，禁止访问基准目录之外的文件: {filename!r}"
        ) from exc

    if must_exist and not candidate.exists():
        raise NotFoundError(f"文件不存在: {candidate}")

    return candidate
