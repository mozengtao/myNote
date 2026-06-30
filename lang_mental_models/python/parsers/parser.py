"""解析层：原始文本行 -> LogEvent 对象。

这里集中所有 split 等"脏活"，让 Pipeline / Aggregator 永远只面对对象。
每个函数都是纯函数：输入文本，输出对象，无副作用，便于单元测试。

原始行格式：timestamp level service message...
"""

from __future__ import annotations

from models.event import LogEvent, RawLine


def parse_line(raw: RawLine) -> LogEvent | None:
    """把一条 RawLine 解析为 LogEvent；格式不合法时返回 None。"""
    fields = raw.text.split()
    if len(fields) < 4:
        return None
    level = fields[1]
    service = fields[2]
    message = " ".join(fields[3:])
    return LogEvent(
        source=raw.source,
        level=level,
        service=service,
        message=message,
    )


def parse_lines(raw_lines: list[RawLine]) -> list[LogEvent]:
    """批量解析，自动跳过非法行。"""
    events: list[LogEvent] = []
    for raw in raw_lines:
        event = parse_line(raw)
        if event is not None:
            events.append(event)
    return events
