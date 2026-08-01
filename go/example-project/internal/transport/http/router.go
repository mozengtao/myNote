// Package http（transport/http）是"传输层适配器"：把 handler 暴露的
// 业务能力挂载到具体的 HTTP 路由上，并套上通用中间件。
// 如果未来要新增 gRPC 或消息队列入口，会新建 transport/grpc、transport/mq，
// 而 handler/service/repository 都不需要改动。
package http

import (
	"net/http"

	"github.com/morrism/go-example-project/internal/handler"
	"github.com/morrism/go-example-project/internal/middleware"
	"github.com/morrism/go-example-project/pkg/logger"
)

// New 组装出完整的 http.Handler：路由 + 中间件链。
// 使用 Go 1.22+ 标准库 ServeMux 的方法路由语法（"POST /users/register"），
// 因此不需要引入 gorilla/mux 或 chi 这类第三方路由库。
func New(userHandler *handler.UserHandler, log *logger.Logger) http.Handler {
	mux := http.NewServeMux()

	mux.HandleFunc("GET /healthz", healthz)
	mux.HandleFunc("POST /users/register", userHandler.Register)
	mux.HandleFunc("POST /users/login", userHandler.Login)
	mux.HandleFunc("GET /users/{id}", userHandler.GetUser)

	// 中间件从外到内依次是 Recovery -> Logging -> mux，
	// 即请求先被 Recovery 兜底、再被 Logging 记录，最后才进入业务路由。
	return chain(mux,
		middleware.Recovery(log),
		middleware.Logging(log),
	)
}

func healthz(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Write([]byte(`{"status":"ok"}`))
}

// chain 按声明顺序把多个中间件包裹在 handler 外层。
func chain(h http.Handler, mws ...func(http.Handler) http.Handler) http.Handler {
	for i := len(mws) - 1; i >= 0; i-- {
		h = mws[i](h)
	}
	return h
}
