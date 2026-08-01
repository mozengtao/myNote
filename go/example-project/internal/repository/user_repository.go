// Package repository 定义数据访问的"契约"（interface），不包含任何具体实现。
//
// 关于接口该放哪里，project_organization.md 第十节的"纯粹派"建议是把接口
// 定义在消费者（service）所在包里；而第十三节的企业级目录把它单独放在
// repository/ 包下。这里选择了后者：当一个仓储接口会被多个上层（service、
// 未来可能的后台任务/CLI）共用时，把接口独立出来可以避免这些消费者互相
// 依赖对方的包。无论放在哪里，核心原则不变——接口属于"调用方视角"，
// 具体实现（persistence/memory、persistence/mysql）反过来依赖并实现它，
// 而不是 service 直接依赖某个具体实现。
package repository

import (
	"context"

	"github.com/morrism/go-example-project/internal/domain"
)

// UserRepository 是 service 层依赖的数据访问契约。
type UserRepository interface {
	Create(ctx context.Context, u *domain.User) error
	FindByID(ctx context.Context, id string) (*domain.User, error)
	FindByUsername(ctx context.Context, username string) (*domain.User, error)
}
