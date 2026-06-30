// Package reporter 是「输出 Service」：把 Report 渲染成 Markdown 并落盘。
// 只负责"对象 -> 输出格式"，想换 HTML / JSON 只需新增一个 reporter。
package reporter

import (
	"fmt"
	"log"
	"os"
	"strings"

	"lang_mental_models/go/internal/model"
)

// Write 渲染 report 并写入 path。
func Write(report model.Report, path string) error {
	content := Render(report)
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		return err
	}
	log.Printf("[REPORTER] markdown report written: %s", path)
	return nil
}

// Render 把 Report 转成 Markdown 字符串（纯函数，便于测试）。
func Render(report model.Report) string {
	a := report.Aggregation
	var b strings.Builder

	fmt.Fprintln(&b, "# 日志聚合报告")
	fmt.Fprintln(&b)
	fmt.Fprintf(&b, "- 来源数：%d\n", len(a.Sources))
	fmt.Fprintf(&b, "- 日志总行数：%d（ERROR: %d / WARN: %d / INFO: %d）\n", a.Total, a.Error, a.Warn, a.Info)
	fmt.Fprintln(&b)

	fmt.Fprintln(&b, "## 各服务告警统计（ERROR + WARN，降序）")
	fmt.Fprintln(&b)
	fmt.Fprintln(&b, "| 服务 | ERROR | WARN | 合计 |")
	fmt.Fprintln(&b, "| --- | --- | --- | --- |")
	for _, s := range a.Services {
		fmt.Fprintf(&b, "| %s | %d | %d | %d |\n", s.Service, s.Error, s.Warn, s.Total())
	}
	fmt.Fprintln(&b)

	fmt.Fprintln(&b, "## Top-5 错误消息")
	fmt.Fprintln(&b)
	fmt.Fprintln(&b, "| 次数 | 服务 | 消息 |")
	fmt.Fprintln(&b, "| --- | --- | --- |")
	for _, m := range a.TopErrors {
		fmt.Fprintf(&b, "| %d | %s | %s |\n", m.Count, m.Service, m.Message)
	}
	fmt.Fprintln(&b)

	fmt.Fprintln(&b, "## 各来源明细")
	fmt.Fprintln(&b)
	fmt.Fprintln(&b, "| 来源 | 行数 | ERROR | WARN | INFO |")
	fmt.Fprintln(&b, "| --- | --- | --- | --- | --- |")
	for _, s := range a.Sources {
		fmt.Fprintf(&b, "| %s | %d | %d | %d | %d |\n", s.Source, s.Lines, s.Error, s.Warn, s.Info)
	}

	return b.String()
}
