#!/usr/bin/env bash
#
# aggregate：主入口。Shell 的「编排」就是一条管道。
#
# 数据全程以「文本行」在阶段之间流动：
#
#     ingest logs/*.log | report > report.md
#                │                  │
#                ▼                  ▼
#        规范化 TSV 文本流     Markdown 报告
#
# report 内部再把同一份文本喂给 stats / topn 得到各个表格视图。
# 真实场景里日志可能来自 journalctl / loki / ssh，这里用 logs/ 下的
# 文本文件代替——对管道而言，数据来源是什么并不重要，它只认「文本」。
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB="$HERE/lib"
LOG_DIR="${1:-$HERE/logs}"
OUT="$HERE/report.md"

echo "[ingest] 读取日志源: ${LOG_DIR}/*.log" >&2

# 核心一行：文本从 ingest 流向 report，最终落成 markdown
"$LIB/ingest.sh" "$LOG_DIR"/*.log | "$LIB/report.sh" >"$OUT"

echo "[done] 报告已生成: ${OUT}" >&2
