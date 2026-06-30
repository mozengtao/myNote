"""分析层：把 Device 列表汇总成 AnalysisResult。

对应指南数据流中的 Analyzer：Device Object -> Analyzer -> AnalysisResult。
它只接收对象、产出对象，不碰 SSH、不碰文本、不碰输出格式。
"""

from __future__ import annotations

from collections import Counter

from models.device import Device
from models.report import AnalysisResult


def analyze(devices: list[Device]) -> AnalysisResult:
    """统计设备总数、接口 up/down 数量与版本分布。"""
    version_counter: Counter[str] = Counter()
    total_interfaces = 0
    total_up = 0
    total_down = 0

    for device in devices:
        version_counter[device.version] += 1
        total_interfaces += len(device.interfaces)
        total_up += device.up_count
        total_down += device.down_count

    return AnalysisResult(
        total_devices=len(devices),
        total_interfaces=total_interfaces,
        total_up=total_up,
        total_down=total_down,
        version_distribution=dict(version_counter),
    )
