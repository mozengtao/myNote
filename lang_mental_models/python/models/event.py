"""日志领域的数据模型。

整个程序传递的是对象，而不是字符串：
    event.level / event.service / event.message
而不是：
    fields[1] / fields[2] / " ".join(fields[3:])

RawLine 是"还没解析"的原始行（带来源），LogEvent 是"已解析"的结构化事件。
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class RawLine:
    """来自某个来源的一条原始日志文本（未解析）。"""

    source: str
    text: str


@dataclass
class LogEvent:
    """一条已解析的结构化日志事件。

    它是 Parser 的产物，也是 Aggregator / Reporter 的输入。
    """

    source: str
    level: str
    service: str
    message: str

    @property
    def is_error(self) -> bool:
        return self.level == "ERROR"

    @property
    def is_warn(self) -> bool:
        return self.level == "WARN"

    @property
    def is_info(self) -> bool:
        return self.level == "INFO"
