// Package middleware 存放横切关注点（cross-cutting concerns）：
// 日志、panic 恢复等所有 HTTP 请求都需要的公共行为，与具体业务路由解耦。
package middleware

import (
	"net/http"
	"time"

	"github.com/morrism/go-example-project/pkg/logger"
)

// Logging 记录每个请求的方法、路径、状态码和耗时。
func Logging(log *logger.Logger) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			start := time.Now()
			sw := &statusWriter{ResponseWriter: w, status: http.StatusOK}

			next.ServeHTTP(sw, r)

			log.Info("%s %s -> %d (%s)", r.Method, r.URL.Path, sw.status, time.Since(start))
		})
	}
}

// statusWriter 包一层 http.ResponseWriter 以便记录实际写出的状态码。
type statusWriter struct {
	http.ResponseWriter
	status int
}

func (w *statusWriter) WriteHeader(status int) {
	w.status = status
	w.ResponseWriter.WriteHeader(status)
}
