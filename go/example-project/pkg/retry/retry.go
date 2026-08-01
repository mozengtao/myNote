// Package retry 提供一个通用的、带固定退避的重试帮助函数。
// 放在 pkg/ 下，因为"重试"是一个与业务无关的通用能力，任何调用外部
// 资源（数据库、RPC、HTTP）的代码都可能用到它。
package retry

import (
	"context"
	"fmt"
	"time"
)

// Do 最多尝试 attempts 次调用 fn，每次失败后等待 backoff 再重试，
// 通过 onRetry 回调把每次尝试的信息暴露出去（本项目里用它打印日志）。
func Do(ctx context.Context, attempts int, backoff time.Duration, fn func() error, onRetry func(attempt int, err error)) error {
	var lastErr error
	for attempt := 1; attempt <= attempts; attempt++ {
		if err := ctx.Err(); err != nil {
			return err
		}

		lastErr = fn()
		if lastErr == nil {
			return nil
		}

		if onRetry != nil {
			onRetry(attempt, lastErr)
		}

		if attempt < attempts {
			select {
			case <-time.After(backoff):
			case <-ctx.Done():
				return ctx.Err()
			}
		}
	}
	return fmt.Errorf("retry: giving up after %d attempts: %w", attempts, lastErr)
}
