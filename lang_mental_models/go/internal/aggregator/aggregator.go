// Package aggregator 是流水线的「汇聚 Service」：持续消费 LogEvent 消息，
// 直到上游 channel 关闭，再汇总成 Aggregation。
//
// 排序规则与 Shell / Python 版本保持一致，确保三种语言产出完全相同的报告：
//   - 服务统计：按 (合计 降序, 服务名 升序)
//   - Top-N 错误：按 (次数 降序, 服务 升序, 消息 升序)
//   - 来源明细：按 来源名 升序
package aggregator

import (
	"sort"

	"lang_mental_models/go/internal/model"
)

// Collect 从 in 接收 LogEvent 直到 channel 关闭，然后返回汇总结果。
// 即使上游是并发乱序到达的，这里按 key 计数 + 最终排序，结果仍然确定。
func Collect(in <-chan model.LogEvent, topN int) model.Aggregation {
	var total, errCnt, warnCnt, infoCnt int
	svc := map[string]*model.ServiceStat{}
	src := map[string]*model.SourceStat{}
	msg := map[[2]string]int{}

	for e := range in {
		total++
		switch {
		case e.IsError():
			errCnt++
		case e.IsWarn():
			warnCnt++
		case e.IsInfo():
			infoCnt++
		}

		if e.IsError() || e.IsWarn() {
			s := svc[e.Service]
			if s == nil {
				s = &model.ServiceStat{Service: e.Service}
				svc[e.Service] = s
			}
			if e.IsError() {
				s.Error++
			} else {
				s.Warn++
			}
		}

		if e.IsError() {
			msg[[2]string{e.Service, e.Message}]++
		}

		ss := src[e.Source]
		if ss == nil {
			ss = &model.SourceStat{Source: e.Source}
			src[e.Source] = ss
		}
		ss.Lines++
		switch {
		case e.IsError():
			ss.Error++
		case e.IsWarn():
			ss.Warn++
		case e.IsInfo():
			ss.Info++
		}
	}

	return model.Aggregation{
		Total:     total,
		Error:     errCnt,
		Warn:      warnCnt,
		Info:      infoCnt,
		Services:  sortServices(svc),
		TopErrors: topErrors(msg, topN),
		Sources:   sortSources(src),
	}
}

func sortServices(m map[string]*model.ServiceStat) []model.ServiceStat {
	out := make([]model.ServiceStat, 0, len(m))
	for _, s := range m {
		out = append(out, *s)
	}
	sort.Slice(out, func(i, j int) bool {
		if out[i].Total() != out[j].Total() {
			return out[i].Total() > out[j].Total()
		}
		return out[i].Service < out[j].Service
	})
	return out
}

func topErrors(m map[[2]string]int, topN int) []model.MessageCount {
	out := make([]model.MessageCount, 0, len(m))
	for k, c := range m {
		out = append(out, model.MessageCount{Service: k[0], Message: k[1], Count: c})
	}
	sort.Slice(out, func(i, j int) bool {
		if out[i].Count != out[j].Count {
			return out[i].Count > out[j].Count
		}
		if out[i].Service != out[j].Service {
			return out[i].Service < out[j].Service
		}
		return out[i].Message < out[j].Message
	})
	if topN >= 0 && topN < len(out) {
		out = out[:topN]
	}
	return out
}

func sortSources(m map[string]*model.SourceStat) []model.SourceStat {
	out := make([]model.SourceStat, 0, len(m))
	for _, s := range m {
		out = append(out, *s)
	}
	sort.Slice(out, func(i, j int) bool {
		return out[i].Source < out[j].Source
	})
	return out
}
