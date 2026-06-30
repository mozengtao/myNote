"""解析层：CLI 文本 -> 模型对象。

这里集中所有 split/regex 等"脏活"，让 Workflow 永远不必关心文本格式。
每个函数都是纯函数：输入文本，输出对象，无副作用，便于单元测试。
"""

from __future__ import annotations

import re

from models.device import Interface

# 形如 "..., Version 15.2(4)E7" 里提取版本号
_VERSION_RE = re.compile(r"Version\s+(\S+)")


def parse_version(text: str) -> str:
    """从 `show version` 输出里解析出版本号。"""
    match = _VERSION_RE.search(text)
    if match:
        return match.group(1).rstrip(",")
    return "unknown"


def parse_interfaces(text: str) -> list[Interface]:
    """从接口表输出里解析出 Interface 列表。"""
    interfaces: list[Interface] = []
    for line in text.splitlines():
        fields = line.split()
        # 跳过表头与空行：表头第一列是 "Interface"
        if len(fields) < 4 or fields[0] == "Interface":
            continue
        name = fields[0]
        status = fields[2]  # Status 列（up/down），不取 Protocol 列
        interfaces.append(Interface(name=name, status=status))
    return interfaces
