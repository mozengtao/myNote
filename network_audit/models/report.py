"""分析结果与报告的数据模型。

数据流：Device 列表 --(Analyzer)--> AnalysisResult --(组装)--> Report --(Reporter)--> report.md
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime

from models.device import Device


@dataclass
class AnalysisResult:
    """对一批设备的汇总分析结果。"""

    total_devices: int = 0
    total_interfaces: int = 0
    total_up: int = 0
    total_down: int = 0
    version_distribution: dict[str, int] = field(default_factory=dict)


@dataclass
class Report:
    """一份完整报告：原始设备明细 + 汇总分析 + 生成时间。

    Report 是一个对象，Reporter 负责把它渲染成 Markdown/HTML/JSON。
    """

    devices: list[Device] = field(default_factory=list)
    analysis: AnalysisResult = field(default_factory=AnalysisResult)
    generated_at: datetime = field(default_factory=datetime.now)
