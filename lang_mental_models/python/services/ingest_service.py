"""服务层：日志采集能力。

Service 有业务含义：它知道"采集"= 遍历配置里的每个来源，把每一行
连同其来源包装成 RawLine 对象交给上层。

它向下使用 LogSource（基础设施），向上为 Pipeline 提供领域方法。
注意：Service 返回的是 RawLine（仍是原始文本）——文本到 LogEvent 的
转换是 Parser 的职责，保持单一职责。
"""

from __future__ import annotations

from pathlib import Path

from config.loader import Config
from infra.source import LogSource
from models.event import RawLine
from utils.logger import get_logger


class IngestService:
    """提供"从多个来源采集原始日志行"这一领域能力。"""

    def __init__(self, config: Config, base_dir: Path):
        self.config = config
        self.base_dir = base_dir
        self.log = get_logger("services.ingest")

    def collect(self) -> list[RawLine]:
        """遍历所有来源，返回带来源标记的原始行列表。"""
        raw_lines: list[RawLine] = []
        for rel in self.config.sources:
            source = LogSource(self.base_dir / rel)
            lines = source.read_lines()
            self.log.info("collected %s: %d lines", source.name, len(lines))
            for text in lines:
                raw_lines.append(RawLine(source=source.name, text=text))
        return raw_lines
