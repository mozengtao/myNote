package middleware

import (
	"net/http"

	"github.com/morrism/go-example-project/pkg/logger"
)

// Recovery 捕获 handler 中的 panic，避免一个请求的崩溃打垮整个进程，
// 并向客户端返回统一的 500 响应，而不是让连接直接断开。
func Recovery(log *logger.Logger) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			defer func() {
				if rec := recover(); rec != nil {
					log.Error("panic recovered: %v", rec)
					http.Error(w, `{"error":"internal server error"}`, http.StatusInternalServerError)
				}
			}()
			next.ServeHTTP(w, r)
		})
	}
}
