#!/usr/bin/env bash
#
# stats：对规范化 TSV 文本流（stdin）做聚合，输出「中间 TSV」。
#
# 只负责「计算」，不负责「排版」（排版交给 report.sh），保持单一职责。
#
# 三种视图：
#   summary  -> 一行: total <TAB> ERROR <TAB> WARN <TAB> INFO
#   services -> 多行: service <TAB> ERROR <TAB> WARN <TAB> total   （按 total 降序、名称升序）
#   sources  -> 多行: source  <TAB> lines <TAB> ERROR <TAB> WARN <TAB> INFO （按来源升序）
#
set -euo pipefail

mode="${1:?usage: stats.sh summary|services|sources}"

case "$mode" in
summary)
	awk -F'\t' '
		{ total++ }
		$2 == "ERROR" { e++ }
		$2 == "WARN"  { w++ }
		$2 == "INFO"  { i++ }
		END { printf "%d\t%d\t%d\t%d\n", total, e + 0, w + 0, i + 0 }
	'
	;;
services)
	awk -F'\t' '
		$2 == "ERROR" { err[$3]++;  svc[$3] = 1 }
		$2 == "WARN"  { warn[$3]++; svc[$3] = 1 }
		END {
			for (s in svc) {
				e = err[s] + 0
				w = warn[s] + 0
				printf "%s\t%d\t%d\t%d\n", s, e, w, e + w
			}
		}
	' | sort -t"$(printf '\t')" -k4,4nr -k1,1
	;;
sources)
	awk -F'\t' '
		{ lines[$1]++; src[$1] = 1 }
		$2 == "ERROR" { e[$1]++ }
		$2 == "WARN"  { w[$1]++ }
		$2 == "INFO"  { i[$1]++ }
		END {
			for (s in src)
				printf "%s\t%d\t%d\t%d\t%d\n", s, lines[s], e[s] + 0, w[s] + 0, i[s] + 0
		}
	' | sort -t"$(printf '\t')" -k1,1
	;;
*)
	echo "unknown mode: $mode" >&2
	exit 2
	;;
esac
