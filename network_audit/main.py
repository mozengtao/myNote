"""程序入口：自顶向下地装配依赖并启动工作流。

main 的职责：
1. 加载配置（Configuration）。
2. 组装各层依赖：Service（含 Infra）、Reporter -> 注入 Workflow。
3. 调用 Workflow.run()，让对象在各层之间流动，最终产出 report.md。

注意 main 里看不到 SSH/regex/markdown 的细节——它只负责"组装与启动"。
"""

from __future__ import annotations

from pathlib import Path

from config.loader import load_config
from reporter.markdown import MarkdownReporter
from services.device_service import DeviceService
from utils.logger import get_logger, setup_logging
from workflow.audit import AuditWorkflow

BASE_DIR = Path(__file__).resolve().parent


def main() -> None:
    setup_logging()
    log = get_logger("main")

    # 1. 配置来自外部
    config = load_config(BASE_DIR / "config" / "config.yaml")
    log.info("loaded config: hosts=%s timeout=%s", config.hosts, config.timeout)

    # 2. 组装依赖（依赖注入）
    service = DeviceService(config)
    reporter = MarkdownReporter(BASE_DIR / config.report_path)
    workflow = AuditWorkflow(config=config, service=service, reporter=reporter)

    # 3. 启动对象流：Config -> Device -> Report -> Markdown
    workflow.run()


if __name__ == "__main__":
    main()
