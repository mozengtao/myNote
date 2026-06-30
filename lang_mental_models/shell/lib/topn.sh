#!/usr/bin/env bash
#
# topn：取出现次数最多的前 N 条「错误消息」。
#
# 经典文本统计三连：filter | awk 计数 | sort | head。
# 输入：规范化 TSV 文本流（stdin）
# 输出：count <TAB> service <TAB> message （按次数降序，键升序）
#
# 用法：... | topn.sh 5
#
set -euo pipefail

n="${1:-5}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 只关心 ERROR：先用 filter 过滤，再按 service+message 计数
"$HERE/filter.sh" ERROR \
	| awk -F'\t' '
		{ key = $3 "\t" $4; cnt[key]++ }
		END { for (k in cnt) printf "%d\t%s\n", cnt[k], k }
	' \
	| sort -t"$(printf '\t')" -k1,1nr -k2 \
	| head -n "$n"
