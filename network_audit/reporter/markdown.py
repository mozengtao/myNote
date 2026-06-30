"""输出层：把 Report 对象渲染成 Markdown 并落盘。

Reporter 只负责"对象 -> 输出格式"。把渲染独立出来后，
想换 HTML/JSON 只需新增一个 Reporter，Workflow 完全不用改。
"""

from __future__ import annotations

from pathlib import Path

from models.report import Report
from utils.logger import get_logger


class MarkdownReporter:
    """把 Report 写成 Markdown 文件。"""

    def __init__(self, output_path: str | Path):
        self.output_path = Path(output_path)
        self.log = get_logger("reporter.markdown")

    def render(self, report: Report) -> str:
        """Report 对象 -> Markdown 字符串（纯函数式，便于测试）。"""
        a = report.analysis
        lines: list[str] = []

        lines.append("# 网络设备审计报告")
        lines.append("")
        lines.append(f"- 生成时间：{report.generated_at:%Y-%m-%d %H:%M:%S}")
        lines.append(f"- 设备总数：{a.total_devices}")
        lines.append(f"- 接口总数：{a.total_interfaces}（up: {a.total_up} / down: {a.total_down}）")
        lines.append("")

        lines.append("## 版本分布")
        lines.append("")
        lines.append("| 版本 | 设备数 |")
        lines.append("| --- | --- |")
        for version, count in sorted(a.version_distribution.items()):
            lines.append(f"| {version} | {count} |")
        lines.append("")

        lines.append("## 设备明细")
        lines.append("")
        lines.append("| 主机 | 版本 | 接口数 | up | down |")
        lines.append("| --- | --- | --- | --- | --- |")
        for device in report.devices:
            lines.append(
                f"| {device.hostname} | {device.version} | "
                f"{len(device.interfaces)} | {device.up_count} | {device.down_count} |"
            )
        lines.append("")

        return "\n".join(lines)

    def write(self, report: Report) -> Path:
        """渲染并写入文件，返回写入路径。"""
        content = self.render(report)
        self.output_path.write_text(content, encoding="utf-8")
        self.log.info("markdown report written: %s", self.output_path)
        return self.output_path
