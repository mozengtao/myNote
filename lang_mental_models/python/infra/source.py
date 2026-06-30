"""基础设施层：日志源。

这一层只负责"从外部世界读取原始文本行"，它不知道什么是
LogEvent / Aggregation / Pipeline。

真实项目里这里可能是 SSH、Loki HTTP API、Kafka consumer：
    for msg in consumer: yield msg.value.decode()

本 demo 用"读取 logs/ 下的文本文件 + 打印读取动作"代替真实接入，
因此无需任何外部系统即可运行。想换成真实数据源？只改这一层。
"""

from __future__ import annotations

from pathlib import Path

from utils.logger import get_logger


class LogSource:
    """一个日志来源。name 取自文件名（不含扩展名），如 app1.log -> app1。"""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self.log = get_logger("infra.source")

    @property
    def name(self) -> str:
        return self.path.stem

    def read_lines(self) -> list[str]:
        """返回该来源的所有原始文本行。"""
        self.log.info("[SOURCE] read %s", self.path)
        text = self.path.read_text(encoding="utf-8")
        return [line for line in text.splitlines() if line.strip()]
