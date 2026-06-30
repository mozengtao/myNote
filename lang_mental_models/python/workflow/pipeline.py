"""编排层：日志聚合流程。

Pipeline 只回答一个问题：做什么？
    ingest -> parse -> aggregate -> report

它不知道怎么读文件、不知道 split、不知道 open()/write()。
它只是把 Service / Parser / Aggregator / Reporter 这些"能力"按顺序串起来，
传递的全程都是对象（RawLine、LogEvent、Aggregation、Report），而不是字符串。
"""

from __future__ import annotations

from analysis.aggregator import aggregate
from config.loader import Config
from models.report import Report
from parsers.parser import parse_lines
from reporter.markdown import MarkdownReporter
from services.ingest_service import IngestService
from utils.logger import get_logger


class AggregationPipeline:
    """日志聚合流程的编排者。"""

    def __init__(self, config: Config, ingest: IngestService, reporter: MarkdownReporter):
        self.config = config
        self.ingest = ingest
        self.reporter = reporter
        self.log = get_logger("workflow.pipeline")

    def run(self) -> Report:
        self.log.info("pipeline start: %d sources", len(self.config.sources))

        raw_lines = self.ingest.collect()          # -> list[RawLine]
        events = parse_lines(raw_lines)            # -> list[LogEvent]
        self.log.info("parsed %d events", len(events))

        aggregation = aggregate(events, self.config.top_n)  # -> Aggregation
        report = Report(aggregation=aggregation)

        self.reporter.write(report)                # Report -> report.md
        self.log.info("pipeline done: report written to %s", self.config.report_path)
        return report
