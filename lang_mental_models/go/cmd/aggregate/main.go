// Command aggregate 装配并启动日志聚合的并发流水线。
//
// 心智模型：每个阶段是一个 Service，struct 作为 Message 在 channel 间流动：
//
//	sources --> [source.Stream] --rawCh--> [parser.Stage 池] --eventCh--> [aggregator.Collect] --> [reporter.Write]
//
// 真实场景里这些 Service 可以拆成进程 / 容器 / Pod，用 RPC / Kafka 连接，
// 代码结构几乎不用改——这正是 Go "Service & Message Flow" 的价值。
package main

import (
	"log"
	"path/filepath"
	"runtime"

	"lang_mental_models/go/internal/aggregator"
	"lang_mental_models/go/internal/model"
	"lang_mental_models/go/internal/parser"
	"lang_mental_models/go/internal/reporter"
	"lang_mental_models/go/internal/source"
)

func main() {
	log.SetFlags(0)

	const topN = 5
	parserWorkers := runtime.NumCPU()

	sources := []string{
		filepath.Join("logs", "app1.log"),
		filepath.Join("logs", "app2.log"),
		filepath.Join("logs", "app3.log"),
	}

	log.Printf("[main] pipeline start: %d sources, %d parser workers", len(sources), parserWorkers)

	// 组装流水线：channel 把各 Service 串起来，数据以消息形式流动。
	rawCh := source.Stream(sources)               // 多源扇入 -> rawCh
	eventCh := parser.Stage(rawCh, parserWorkers) // Parser 池 -> eventCh
	agg := aggregator.Collect(eventCh, topN)      // 汇聚 -> Aggregation

	report := model.Report{Aggregation: agg}
	if err := reporter.Write(report, "report.md"); err != nil {
		log.Fatalf("write report: %v", err)
	}

	log.Printf("[main] pipeline done: %d events aggregated", agg.Total)
}
