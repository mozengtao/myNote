"""输出层：把 Report 对象渲染成 Markdown 并落盘。

Reporter 只负责"对象 -> 输出格式"。把渲染独立出来后，
想换 HTML/JSON 只需新增一个 Reporter，Pipeline 完全不用改。
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
        a = report.aggregation
        lines: list[str] = []

        lines.append("# 日志聚合报告")
        lines.append("")
        lines.append(f"- 来源数：{len(a.sources)}")
        lines.append(f"- 日志总行数：{a.total}（ERROR: {a.error} / WARN: {a.warn} / INFO: {a.info}）")
        lines.append("")

        lines.append("## 各服务告警统计（ERROR + WARN，降序）")
        lines.append("")
        lines.append("| 服务 | ERROR | WARN | 合计 |")
        lines.append("| --- | --- | --- | --- |")
        for s in a.services:
            lines.append(f"| {s.service} | {s.error} | {s.warn} | {s.total} |")
        lines.append("")

        lines.append("## Top-5 错误消息")
        lines.append("")
        lines.append("| 次数 | 服务 | 消息 |")
        lines.append("| --- | --- | --- |")
        for m in a.top_errors:
            lines.append(f"| {m.count} | {m.service} | {m.message} |")
        lines.append("")

        lines.append("## 各来源明细")
        lines.append("")
        lines.append("| 来源 | 行数 | ERROR | WARN | INFO |")
        lines.append("| --- | --- | --- | --- | --- |")
        for s in a.sources:
            lines.append(f"| {s.source} | {s.lines} | {s.error} | {s.warn} | {s.info} |")

        return "\n".join(lines) + "\n"

    def write(self, report: Report) -> Path:
        """渲染并写入文件，返回写入路径。"""
        content = self.render(report)
        self.output_path.write_text(content, encoding="utf-8")
        self.log.info("markdown report written: %s", self.output_path)
        return self.output_path
