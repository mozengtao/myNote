// Package source 是「基础设施 Service」：只负责把外部文本读成 RawLine 消息。
//
// 真实场景里来源可能是 SSH / Loki / Kafka consumer，这里读 logs/ 下的
// 文件代替。对下游而言，数据来自哪里并不重要，它只认 channel 里的消息。
package source

import (
	"bufio"
	"log"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"lang_mental_models/go/internal/model"
)

// Stream 为每个来源启动一个 goroutine 并发读取，并把所有行「扇入」(fan-in)
// 到同一个 channel。返回的 channel 在所有来源读完后自动关闭。
func Stream(paths []string) <-chan model.RawLine {
	out := make(chan model.RawLine)

	var wg sync.WaitGroup
	for _, p := range paths {
		wg.Add(1)
		go func(path string) {
			defer wg.Done()
			readOne(path, out)
		}(p)
	}

	// 单独的 goroutine 负责在全部来源读完后关闭 channel，
	// 让下游可以用 range 自然结束。
	go func() {
		wg.Wait()
		close(out)
	}()

	return out
}

func readOne(path string, out chan<- model.RawLine) {
	name := strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))

	f, err := os.Open(path)
	if err != nil {
		log.Printf("[SOURCE] open %s failed: %v", path, err)
		return
	}
	defer f.Close()
	log.Printf("[SOURCE] read %s", path)

	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" {
			continue
		}
		out <- model.RawLine{Source: name, Text: line}
	}
}
