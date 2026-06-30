#!/usr/bin/env bash
#
# ingest：把多个日志源合并成「规范化文本流」。
#
# 心智模型：Shell 的世界里一切都是文本。这里把杂乱的原始日志统一成
# 一种规范化 TSV 行，后续每一个阶段都只面对这种文本行：
#
#     source <TAB> level <TAB> service <TAB> message
#
# 原始行格式：timestamp level service message...
# 用法：ingest.sh logs/*.log
#
set -euo pipefail

for f in "$@"; do
	[ -f "$f" ] || continue
	src=$(basename "$f" .log)
	awk -v src="$src" '
		NF >= 4 {
			level   = $2
			service = $3
			# message = 第 4 个字段起的剩余部分
			msg = $4
			for (i = 5; i <= NF; i++)
				msg = msg " " $i
			printf "%s\t%s\t%s\t%s\n", src, level, service, msg
		}
	' "$f"
done
