"""聚合结果与报告的数据模型。

数据流：
    LogEvent 列表 --(Aggregator)--> Aggregation --(组装)--> Report --(Reporter)--> report.md
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class ServiceStat:
    """单个服务的告警统计。"""

    service: str
    error: int = 0
    warn: int = 0

    @property
    def total(self) -> int:
        return self.error + self.warn


@dataclass
class SourceStat:
    """单个来源的级别分布。"""

    source: str
    lines: int = 0
    error: int = 0
    warn: int = 0
    info: int = 0


@dataclass
class MessageCount:
    """某条错误消息的出现次数。"""

    service: str
    message: str
    count: int


@dataclass
class Aggregation:
    """对一批日志事件的汇总分析结果。"""

    total: int = 0
    error: int = 0
    warn: int = 0
    info: int = 0
    services: list[ServiceStat] = field(default_factory=list)
    top_errors: list[MessageCount] = field(default_factory=list)
    sources: list[SourceStat] = field(default_factory=list)


@dataclass
class Report:
    """一份完整报告：汇总分析结果。

    Report 是一个对象，Reporter 负责把它渲染成 Markdown / HTML / JSON。
    """

    aggregation: Aggregation = field(default_factory=Aggregation)
