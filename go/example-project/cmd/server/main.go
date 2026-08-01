// cmd/server 是本项目唯一的可执行入口：只做命令行参数解析和调用
// app.Run，不包含任何业务逻辑。如果未来要新增 cmd/cli 或 cmd/worker，
// 它们会复用同一套 internal/ 代码，而不会重复实现业务逻辑。
package main

import (
	"context"
	"flag"
	"log"
	"os/signal"
	"syscall"

	"github.com/morrism/go-example-project/internal/app"
	"github.com/morrism/go-example-project/internal/config"
)

func main() {
	configPath := flag.String("config", "configs/dev.json", "path to the JSON config file")
	flag.Parse()

	cfg, err := config.Load(*configPath)
	if err != nil {
		log.Fatalf("failed to load config: %v", err)
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()

	if err := app.New(cfg).Run(ctx); err != nil {
		log.Fatalf("server exited with error: %v", err)
	}
}
