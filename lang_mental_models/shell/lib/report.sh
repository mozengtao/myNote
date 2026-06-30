#!/usr/bin/env bash
#
# report：把规范化 TSV 文本流（stdin）汇编成 Markdown 报告（stdout）。
#
# report 只负责「排版」：把 stats / topn 算出来的中间 TSV 拼成表格。
# 由于要对同一份数据做多个视图，这里先把 stdin 缓存到临时文件，
# 再让各个聚合器分别读取——这正是 Shell 处理「一份文本、多种视图」的常用手法。
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
cat >"$tmp" # 缓存规范化文本流

# 概要：一行四列
read -r total err warn info < <("$HERE/stats.sh" summary <"$tmp")
src_count=$("$HERE/stats.sh" sources <"$tmp" | wc -l | tr -d ' ')

{
	echo "# 日志聚合报告"
	echo
	echo "- 来源数：${src_count}"
	echo "- 日志总行数：${total}（ERROR: ${err} / WARN: ${warn} / INFO: ${info}）"
	echo
	echo "## 各服务告警统计（ERROR + WARN，降序）"
	echo
	echo "| 服务 | ERROR | WARN | 合计 |"
	echo "| --- | --- | --- | --- |"
	"$HERE/stats.sh" services <"$tmp" | while IFS=$'\t' read -r s e w t; do
		echo "| ${s} | ${e} | ${w} | ${t} |"
	done
	echo
	echo "## Top-5 错误消息"
	echo
	echo "| 次数 | 服务 | 消息 |"
	echo "| --- | --- | --- |"
	"$HERE/topn.sh" 5 <"$tmp" | while IFS=$'\t' read -r c s m; do
		echo "| ${c} | ${s} | ${m} |"
	done
	echo
	echo "## 各来源明细"
	echo
	echo "| 来源 | 行数 | ERROR | WARN | INFO |"
	echo "| --- | --- | --- | --- | --- |"
	"$HERE/stats.sh" sources <"$tmp" | while IFS=$'\t' read -r s l e w i; do
		echo "| ${s} | ${l} | ${e} | ${w} | ${i} |"
	done
}
