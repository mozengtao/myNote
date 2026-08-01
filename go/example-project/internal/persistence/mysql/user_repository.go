// Package mysql 演示"同一个 repository.UserRepository 接口的另一种实现"。
//
// 这里 **没有** 引入真正的 MySQL 驱动（比如 go-sql-driver/mysql），
// 因为那属于"具体复杂实现"，按需求用 fmt.Printf 打印代替真实的 SQL 执行
// 和网络交互；同时用 pkg/retry 模拟"数据库连接偶发失败、自动重试"的常见模式。
//
// 重点不是这段代码本身有没有连上数据库，而是展示：只要都实现了
// repository.UserRepository 接口，internal/app 里就可以通过一行配置
// 在 memory 和 mysql 两种实现之间切换，而 service/handler 完全不用改。
package mysql

import (
	"context"
	"fmt"
	"time"

	"github.com/morrism/go-example-project/internal/domain"
	"github.com/morrism/go-example-project/internal/repository"
	"github.com/morrism/go-example-project/pkg/retry"
)

type userRepository struct {
	dsn string
}

// New 返回一个"看起来像"连接到 MySQL 的 UserRepository 实现。
// dsn 只是被打印出来，说明真实实现会用它做 sql.Open("mysql", dsn)。
func New(dsn string) repository.UserRepository {
	fmt.Printf("[mysql] would connect using DSN=%q (real code: sql.Open(\"mysql\", dsn))\n", dsn)
	return &userRepository{dsn: dsn}
}

func (r *userRepository) Create(ctx context.Context, u *domain.User) error {
	attempt := 0
	return retry.Do(ctx, 3, 100*time.Millisecond, func() error {
		attempt++
		if attempt < 2 {
			// 模拟第一次连接偶发失败，触发重试逻辑。
			return fmt.Errorf("simulated connection refused")
		}
		fmt.Printf(
			"[mysql] would execute: INSERT INTO users(id, username, password_hash, created_at) VALUES (%q, %q, %q, %q)\n",
			u.ID, u.Username, u.PasswordHash, u.CreatedAt.Format(time.RFC3339),
		)
		return nil
	}, func(attempt int, err error) {
		fmt.Printf("[mysql] attempt %d failed: %v, retrying...\n", attempt, err)
	})
}

func (r *userRepository) FindByID(ctx context.Context, id string) (*domain.User, error) {
	fmt.Printf("[mysql] would execute: SELECT id, username, password_hash, created_at FROM users WHERE id = %q\n", id)
	// 真实实现会在这里 rows.Scan(...) 填充 domain.User；
	// 这个占位实现没有真正存储数据，因此总是返回"未找到"。
	return nil, domain.ErrUserNotFound
}

func (r *userRepository) FindByUsername(ctx context.Context, username string) (*domain.User, error) {
	fmt.Printf("[mysql] would execute: SELECT id, username, password_hash, created_at FROM users WHERE username = %q\n", username)
	return nil, domain.ErrUserNotFound
}
