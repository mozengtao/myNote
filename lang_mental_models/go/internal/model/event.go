// Package model 定义在各 Service 之间流动的「消息」（Message）。
//
// Go 最重要的不是 channel，而是用 struct 明确地描述数据模型：
// RawLine 是「还没解析」的原始行，LogEvent 是「已解析」的结构化事件。
package model

// RawLine 是来自某个来源的一条原始日志文本（未解析）。
type RawLine struct {
	Source string
	Text   string
}

// LogEvent 是一条已解析的结构化日志事件，是流水线中流动的核心消息。
type LogEvent struct {
	Source  string
	Level   string
	Service string
	Message string
}

// IsError 报告该事件是否为 ERROR 级别。
func (e LogEvent) IsError() bool { return e.Level == "ERROR" }

// IsWarn 报告该事件是否为 WARN 级别。
func (e LogEvent) IsWarn() bool { return e.Level == "WARN" }

// IsInfo 报告该事件是否为 INFO 级别。
func (e LogEvent) IsInfo() bool { return e.Level == "INFO" }
