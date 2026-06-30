"""分析层：把 LogEvent 列表汇总成 Aggregation。

它只接收对象、产出对象，不碰文件、不碰文本格式、不碰输出排版。
排序规则与 Shell / Go 版本保持一致，确保三种语言产出完全相同的报告：
- 服务统计：按 (合计 降序, 服务名 升序)
- Top-N 错误：按 (次数 降序, (服务, 消息) 升序)
- 来源明细：按 来源名 升序
"""

from __future__ import annotations

from collections import Counter

from models.event import LogEvent
from models.report import Aggregation, MessageCount, ServiceStat, SourceStat


def aggregate(events: list[LogEvent], top_n: int = 5) -> Aggregation:
    """统计级别分布、各服务告警、Top-N 错误消息与各来源明细。"""
    level_counter: Counter[str] = Counter(e.level for e in events)

    # 各服务告警（仅 ERROR / WARN）
    services: dict[str, ServiceStat] = {}
    for e in events:
        if e.is_error or e.is_warn:
            stat = services.setdefault(e.service, ServiceStat(service=e.service))
            if e.is_error:
                stat.error += 1
            else:
                stat.warn += 1
    service_list = sorted(services.values(), key=lambda s: (-s.total, s.service))

    # Top-N 错误消息（仅 ERROR，按 service+message 归并）
    message_counter: Counter[tuple[str, str]] = Counter()
    for e in events:
        if e.is_error:
            message_counter[(e.service, e.message)] += 1
    ranked = sorted(message_counter.items(), key=lambda kv: (-kv[1], kv[0]))
    top_errors = [
        MessageCount(service=service, message=message, count=count)
        for (service, message), count in ranked[:top_n]
    ]

    # 各来源明细
    sources: dict[str, SourceStat] = {}
    for e in events:
        stat = sources.setdefault(e.source, SourceStat(source=e.source))
        stat.lines += 1
        if e.is_error:
            stat.error += 1
        elif e.is_warn:
            stat.warn += 1
        elif e.is_info:
            stat.info += 1
    source_list = sorted(sources.values(), key=lambda s: s.source)

    return Aggregation(
        total=len(events),
        error=level_counter["ERROR"],
        warn=level_counter["WARN"],
        info=level_counter["INFO"],
        services=service_list,
        top_errors=top_errors,
        sources=source_list,
    )
