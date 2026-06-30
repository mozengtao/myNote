"""程序入口：自顶向下地装配依赖并启动流水线。

main 的职责：
1. 加载配置（Configuration）。
2. 组装各层依赖：IngestService（含 Infra）、Reporter -> 注入 Pipeline。
3. 调用 Pipeline.run()，让对象在各层之间流动，最终产出 report.md。

注意 main 里看不到读文件/split/markdown 的细节——它只负责"组装与启动"。
"""

from __future__ import annotations

from pathlib import Path

from config.loader import load_config
from reporter.markdown import MarkdownReporter
from services.ingest_service import IngestService
from utils.logger import get_logger, setup_logging
from workflow.pipeline import AggregationPipeline

BASE_DIR = Path(__file__).resolve().parent


def main() -> None:
    setup_logging()
    log = get_logger("main")

    # 1. 配置来自外部
    config = load_config(BASE_DIR / "config" / "config.yaml")
    log.info("loaded config: sources=%s top_n=%s", config.sources, config.top_n)

    # 2. 组装依赖（依赖注入）
    ingest = IngestService(config, base_dir=BASE_DIR)
    reporter = MarkdownReporter(BASE_DIR / config.report_path)
    pipeline = AggregationPipeline(config=config, ingest=ingest, reporter=reporter)

    # 3. 启动对象流：Config -> RawLine -> LogEvent -> Aggregation -> Report -> Markdown
    pipeline.run()


if __name__ == "__main__":
    main()
