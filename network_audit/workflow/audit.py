"""编排层：审计工作流。

Workflow 只回答一个问题：做什么？
    collect -> parse -> analyze -> report

它不知道 SSH、不知道 regex、不知道 open()/write()。
它只是把 Service / Parser / Analyzer / Reporter 这些"能力"按顺序串起来，
传递的全程都是对象（Device、Report），而不是字符串。
"""

from __future__ import annotations

from analysis.analyzer import analyze
from config.loader import Config
from models.device import Device
from models.report import Report
from parsers import parser
from reporter.markdown import MarkdownReporter
from services.device_service import DeviceService
from utils.logger import get_logger


class AuditWorkflow:
    """网络设备审计流程的编排者。"""

    def __init__(self, config: Config, service: DeviceService, reporter: MarkdownReporter):
        self.config = config
        self.service = service
        self.reporter = reporter
        self.log = get_logger("workflow.audit")

    def run(self) -> Report:
        self.log.info("audit start: %d hosts", len(self.config.hosts))

        devices = self._collect()           # -> list[Device]
        result = analyze(devices)           # -> AnalysisResult
        report = Report(devices=devices, analysis=result)

        self.reporter.write(report)         # Report -> report.md
        self.log.info("audit done: report written to %s", self.config.report_path)
        return report

    def _collect(self) -> list[Device]:
        """对每台主机采集原始文本并解析为 Device 对象。"""
        devices: list[Device] = []
        for host in self.config.hosts:
            version_text = self.service.collect_version(host)
            interface_text = self.service.collect_interfaces(host)

            device = Device(
                hostname=host,
                version=parser.parse_version(version_text),
                interfaces=parser.parse_interfaces(interface_text),
            )
            self.log.info(
                "collected %s: version=%s interfaces=%d",
                host, device.version, len(device.interfaces),
            )
            devices.append(device)
        return devices
