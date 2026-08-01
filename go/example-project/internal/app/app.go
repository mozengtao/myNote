// Package app 是唯一知道"所有零件"的地方：它读取配置、决定用哪个
// repository 实现、把 repository 注入 service、把 service 注入 handler、
// 把 handler 挂载到 router 上，最后启动/优雅关闭 HTTP 服务器。
//
// 这就是 project_organization.md 里说的"应用启动、依赖注入"——
// 除了 cmd/server/main.go，没有任何其他包知道完整的组装过程。
package app

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/morrism/go-example-project/internal/config"
	"github.com/morrism/go-example-project/internal/handler"
	"github.com/morrism/go-example-project/internal/persistence/memory"
	"github.com/morrism/go-example-project/internal/persistence/mysql"
	"github.com/morrism/go-example-project/internal/repository"
	"github.com/morrism/go-example-project/internal/service"
	transporthttp "github.com/morrism/go-example-project/internal/transport/http"
	"github.com/morrism/go-example-project/pkg/logger"
)

// App 持有组装好的 HTTP 服务器，对外只暴露 Run。
type App struct {
	cfg    *config.Config
	log    *logger.Logger
	server *http.Server
}

// New 根据配置组装出一个完整可运行的 App（依赖注入的具体发生地）。
func New(cfg *config.Config) *App {
	log := logger.New(parseLevel(cfg.LogLevel))

	repo := newRepository(cfg, log)
	svc := service.New(repo, log)
	h := handler.New(svc, log)
	router := transporthttp.New(h, log)

	server := &http.Server{
		Addr:              fmt.Sprintf(":%d", cfg.Port),
		Handler:           router,
		ReadHeaderTimeout: 5 * time.Second,
	}

	return &App{cfg: cfg, log: log, server: server}
}

// newRepository 是本项目里唯一根据配置在多个 repository 实现之间做选择的地方；
// service/handler 完全不知道这个选择是怎么做出来的。
func newRepository(cfg *config.Config, log *logger.Logger) repository.UserRepository {
	switch cfg.PersistenceDriver {
	case config.DriverMySQL:
		log.Info("using mysql persistence driver (stub implementation)")
		return mysql.New(cfg.MySQLDSN)
	default:
		log.Info("using in-memory persistence driver")
		return memory.New()
	}
}

// Run 启动 HTTP 服务器，并在 ctx 被取消（比如收到 SIGINT/SIGTERM）时优雅关闭：
// 停止接收新连接，等待正在处理的请求自然结束，而不是粗暴地掐断进程。
func (a *App) Run(ctx context.Context) error {
	errCh := make(chan error, 1)

	go func() {
		a.log.Info("server listening on %s", a.server.Addr)
		if err := a.server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			errCh <- err
			return
		}
		errCh <- nil
	}()

	select {
	case err := <-errCh:
		return err
	case <-ctx.Done():
		a.log.Info("shutdown signal received, draining in-flight requests...")
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := a.server.Shutdown(shutdownCtx); err != nil {
			return fmt.Errorf("graceful shutdown failed: %w", err)
		}
		a.log.Info("server stopped cleanly")
		return nil
	}
}

func parseLevel(level string) logger.Level {
	switch level {
	case "debug":
		return logger.LevelDebug
	case "error":
		return logger.LevelError
	default:
		return logger.LevelInfo
	}
}
