package model

// ServiceStat 是单个服务的告警统计。
type ServiceStat struct {
	Service string
	Error   int
	Warn    int
}

// Total 返回 ERROR + WARN 的合计。
func (s ServiceStat) Total() int { return s.Error + s.Warn }

// SourceStat 是单个来源的级别分布。
type SourceStat struct {
	Source string
	Lines  int
	Error  int
	Warn   int
	Info   int
}

// MessageCount 是某条错误消息的出现次数。
type MessageCount struct {
	Service string
	Message string
	Count   int
}

// Aggregation 是对一批日志事件的汇总分析结果。
type Aggregation struct {
	Total     int
	Error     int
	Warn      int
	Info      int
	Services  []ServiceStat
	TopErrors []MessageCount
	Sources   []SourceStat
}

// Report 是一份完整报告。Reporter 负责把它渲染成 Markdown / HTML / JSON。
type Report struct {
	Aggregation Aggregation
}
