#!/usr/bin/env bash
#
# filter：从规范化 TSV 文本流（stdin）里保留指定级别的行。
#
# 这是一个标准的「文本过滤器」：读 stdin，写 stdout，可被任意管道组合。
#   ingest.sh logs/*.log | filter.sh ERROR
#   ingest.sh logs/*.log | filter.sh 'ERROR|WARN'
#
# 默认保留 ERROR 与 WARN（告警类）。
#
set -euo pipefail

pattern="${1:-ERROR|WARN}"

awk -F'\t' -v pat="$pattern" '$2 ~ ("^(" pat ")$")'
