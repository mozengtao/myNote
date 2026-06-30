// Package parser 是「解析 Service」：把 RawLine 消息转换成 LogEvent 消息。
//
// 它以 worker pool 形式运行：多个 goroutine 同时从输入 channel 消费、
// 向输出 channel 生产，这是 Go 流水线里典型的「并发阶段」。
package parser

import (
	"log"
	"strings"
	"sync"

	"lang_mental_models/go/internal/model"
)

// Stage 启动 workers 个 goroutine 组成 Parser 池：
// 从 in 读取 RawLine，解析为 LogEvent 后写入返回的 channel。
// 当上游 in 关闭且所有 worker 退出后，返回的 channel 自动关闭。
func Stage(in <-chan model.RawLine, workers int) <-chan model.LogEvent {
	if workers < 1 {
		workers = 1
	}
	out := make(chan model.LogEvent)

	var wg sync.WaitGroup
	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for raw := range in {
				if ev, ok := parse(raw); ok {
					out <- ev
				}
			}
		}()
	}

	go func() {
		wg.Wait()
		close(out)
	}()

	return out
}

// parse 把一条 RawLine 解析为 LogEvent。
// 原始行格式：timestamp level service message...
func parse(raw model.RawLine) (model.LogEvent, bool) {
	fields := strings.Fields(raw.Text)
	if len(fields) < 4 {
		log.Printf("[PARSER] skip malformed line from %s: %q", raw.Source, raw.Text)
		return model.LogEvent{}, false
	}
	return model.LogEvent{
		Source:  raw.Source,
		Level:   fields[1],
		Service: fields[2],
		Message: strings.Join(fields[3:], " "),
	}, true
}
