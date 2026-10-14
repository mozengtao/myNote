"""Service 层：命令控制台"文件查看面板"背后的受限文件读取规则。

为什么需要这个模块
------------------
示例 6 除了"执行命令"之外，还提供一个只读的文件查看面板，方便用户在不
打开终端的情况下查看项目里的文档、日志、配置文件。这同样是一个"界面上有
一个看起来像'随便填个文件名'的输入框，但背后必须有严格边界"的场景——本
模块就是那条边界：只允许列出/读取某个指定基准目录下、扩展名在白名单内的
文件，并且把真正的越界检查全权委托给已经在别处验证过的
`services.path_guard.confine_path()`，自己不再重新发明一套路径安全逻辑。

心智模型
--------
`list_viewable_files()` 负责"告诉 UI 有哪些文件可以选"——它只做浅层遍历
（不递归进子目录），并按扩展名过滤，返回的是**文件名**而不是完整路径，
UI 层应当只把这些文件名原样传回 `read_text_file()`，不应该自己拼接路径。

`read_text_file()` 负责"真正把文件内容读出来"，它的安全检查分两层：

1. 扩展名检查（本模块自己做）：如果调用方传入的文件名扩展名不在
   `allowed_suffixes` 里，直接拒绝——这是一层"防御性收紧"，防止将来有代码
   路径绕过 `list_viewable_files()` 的过滤，直接拿一个任意扩展名的文件名
   调用 `read_text_file()`。
2. 路径越界检查（委托给 `path_guard.confine_path()`）：确保最终解析出的
   绝对路径确实落在 `base_dir` 内部，拒绝 `../`、绝对路径等任何越界尝试。

读取到文件内容之后，还会按字节数截断到 `max_bytes` 以内，并明确告诉调用方
"有没有被截断"，由 UI 决定要不要提示用户"内容已截断"。
"""

from __future__ import annotations

from pathlib import Path
from typing import Optional

from services.errors import PermissionDeniedError
from services.path_guard import confine_path

#: `read_text_file()` 的默认最大读取字节数（200KB）。超过这个大小只会读取
#: 文件的前 `max_bytes` 字节，不会把整个大文件都加载进内存。
DEFAULT_MAX_BYTES = 200_000


def list_viewable_files(base_dir: Path, allowed_suffixes: tuple[str, ...]) -> list[str]:
    """列出 `base_dir` 下（仅浅层遍历，不递归子目录）符合扩展名白名单的文件名。

    Args:
        base_dir: 允许查看的基准目录，调用方需保证它本身是一个存在的目录。
        allowed_suffixes: 允许的扩展名集合，例如 `('.md', '.txt')`；比较时
            大小写敏感，与 `Path.suffix` 的返回值（包含前导 `.`）直接比较。

    Returns:
        按文件名排序后的文件名列表（不含路径、不含目录项）。
    """
    base = base_dir.resolve()
    names = [
        entry.name
        for entry in base.iterdir()
        if entry.is_file() and entry.suffix in allowed_suffixes
    ]
    return sorted(names)


def read_text_file(
    filename: str,
    base_dir: Path,
    *,
    max_bytes: int = DEFAULT_MAX_BYTES,
    allowed_suffixes: Optional[tuple[str, ...]] = None,
) -> tuple[str, bool]:
    """读取 `base_dir` 下某个文件的文本内容，并限制最大读取字节数。

    Args:
        filename: 文件名（可以带子目录，但不能是绝对路径），通常应当来自
            `list_viewable_files()` 的返回值。
        base_dir: 允许访问的基准目录。
        max_bytes: 最多读取的字节数；超过时只返回前 `max_bytes` 字节解码
            后的文本，并把第二个返回值标记为 `True`。
        allowed_suffixes: 如果提供，会在做路径越界检查之前先校验扩展名，
            不在列表内直接拒绝。传 `None` 表示不做扩展名限制（调用方自己
            保证只会传入已经过滤好的文件名）。本项目示例 6 的 UI 会显式
            传入与 `list_viewable_files()` 相同的扩展名白名单，做到
            "双重校验"而不是只依赖下拉框本身的过滤。

    Returns:
        一个 `(文本内容, 是否被截断)` 二元组。

    Raises:
        PermissionDeniedError: 扩展名不在 `allowed_suffixes` 内；或者
            `filename` 是绝对路径，或解析后的路径逃逸出了 `base_dir`
            （由 `path_guard.confine_path()` 负责判定）。
        NotFoundError: 解析出的路径不存在（由 `path_guard.confine_path()`
            负责判定）。
    """
    resolved_path = confine_path(filename, base_dir, must_exist=True)

    if allowed_suffixes is not None and resolved_path.suffix not in allowed_suffixes:
        raise PermissionDeniedError(
            f"不允许读取该扩展名的文件：{filename!r}（仅允许 {allowed_suffixes!r}）"
        )

    data = resolved_path.read_bytes()
    truncated = len(data) > max_bytes
    clipped = data[:max_bytes]
    # 截断点可能落在一个多字节 UTF-8 字符中间，用 errors='replace' 避免
    # 因为解码失败而让一次"查看文件"操作直接抛出意料之外的异常。
    text = clipped.decode("utf-8", errors="replace")
    return text, truncated
